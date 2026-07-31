#include "dice_set_archive.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "dice_storage.h"
#include "esp_log.h"
#include "zlib.h"

#define SETS_ROOT DICE_STORAGE_MOUNT_POINT "/templates"
#define CACHE_ROOT DICE_STORAGE_MOUNT_POINT "/set_cache"
#define COPY_BUFFER 4096

static const char *TAG = "set_archive";

static uint16_t le16(const uint8_t *p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }
static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

static bool safe_name(const char *name)
{
    return name != NULL && name[0] != '\0' && name[0] != '/' && name[0] != '\\' &&
           strstr(name, "..") == NULL && strchr(name, ':') == NULL;
}

static bool mkdirs_for_file(const char *path)
{
    char temp[384];
    snprintf(temp, sizeof(temp), "%s", path);
    for (char *p = temp + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(temp, 0775) != 0 && errno != EEXIST) return false;
            *p = '/';
        }
    }
    return true;
}

static bool inflate_entry(FILE *zip, FILE *out, uint32_t compressed, uint32_t expected)
{
    uint8_t inbuf[COPY_BUFFER];
    uint8_t outbuf[COPY_BUFFER];
    z_stream stream = {0};
    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) return false;
    uint32_t remaining = compressed;
    uint32_t written = 0;
    int status = Z_OK;
    while (remaining > 0 && status != Z_STREAM_END) {
        size_t chunk = remaining > sizeof(inbuf) ? sizeof(inbuf) : remaining;
        if (fread(inbuf, 1, chunk, zip) != chunk) { inflateEnd(&stream); return false; }
        remaining -= (uint32_t)chunk;
        stream.next_in = inbuf;
        stream.avail_in = (uInt)chunk;
        do {
            stream.next_out = outbuf;
            stream.avail_out = sizeof(outbuf);
            status = inflate(&stream, Z_NO_FLUSH);
            if (status != Z_OK && status != Z_STREAM_END) { inflateEnd(&stream); return false; }
            size_t produced = sizeof(outbuf) - stream.avail_out;
            if (produced && fwrite(outbuf, 1, produced, out) != produced) { inflateEnd(&stream); return false; }
            written += (uint32_t)produced;
        } while (stream.avail_in > 0);
    }
    inflateEnd(&stream);
    return status == Z_STREAM_END && written == expected;
}

static bool metadata_entry(const char *relative)
{
    const char *extension = strrchr(relative, '.');
    return extension != NULL &&
           (strcasecmp(extension, ".json") == 0 ||
            strcasecmp(extension, ".lua") == 0);
}

static bool extract_archive(
    const char *archive_path,
    const char *destination,
    bool metadata_only)
{
    FILE *zip = fopen(archive_path, "rb");
    if (!zip) return false;
    uint8_t header[30];
    bool ok = true;
    while (fread(header, 1, 4, zip) == 4) {
        uint32_t signature = le32(header);
        if (signature == 0x02014b50u || signature == 0x06054b50u) break;
        if (signature != 0x04034b50u || fread(header + 4, 1, 26, zip) != 26) { ok = false; break; }
        uint16_t flags = le16(header + 6);
        uint16_t method = le16(header + 8);
        uint32_t compressed = le32(header + 18);
        uint32_t uncompressed = le32(header + 22);
        uint16_t name_len = le16(header + 26);
        uint16_t extra_len = le16(header + 28);
        if ((flags & 0x0008u) != 0 || name_len == 0 || name_len > 240) { ok = false; break; }
        char name[256];
        if (fread(name, 1, name_len, zip) != name_len) { ok = false; break; }
        name[name_len] = '\0';
        if (!safe_name(name) || fseek(zip, extra_len, SEEK_CUR) != 0) { ok = false; break; }
        const char *relative = strchr(name, '/');
        relative = relative ? relative + 1 : name;
        if (*relative == '\0') continue;
        if (metadata_only && !metadata_entry(relative)) {
            if (fseek(zip, compressed, SEEK_CUR) != 0) {
                ok = false;
                break;
            }
            continue;
        }
        char output[384];
        snprintf(output, sizeof(output), "%s/%s", destination, relative);
        size_t n = strlen(relative);
        if (relative[n - 1] == '/') { if (mkdirs_for_file(output) && mkdir(output, 0775) != 0 && errno != EEXIST) ok = false; continue; }
        if (!mkdirs_for_file(output)) { ok = false; break; }
        FILE *out = fopen(output, "wb");
        if (!out) { ok = false; break; }
        if (method == 0) {
            uint8_t buffer[COPY_BUFFER];
            uint32_t remaining = compressed;
            while (remaining > 0) {
                size_t chunk = remaining > sizeof(buffer) ? sizeof(buffer) : remaining;
                if (fread(buffer, 1, chunk, zip) != chunk || fwrite(buffer, 1, chunk, out) != chunk) { ok = false; break; }
                remaining -= (uint32_t)chunk;
            }
        } else if (method == 8) {
            ok = inflate_entry(zip, out, compressed, uncompressed);
        } else {
            ok = false;
        }
        fclose(out);
        if (!ok) break;
    }
    fclose(zip);
    return ok;
}

static bool ends_with_set(const char *name)
{
    size_t n = strlen(name);
    return n > 4 && strcasecmp(name + n - 4, ".set") == 0;
}

static bool path_is_directory(const char *path)
{
    struct stat status;
    return stat(path, &status) == 0 && S_ISDIR(status.st_mode);
}

static bool archive_is_prepared(
    const char *source,
    const char *target)
{
    char marker[384];
    snprintf(marker, sizeof(marker), "%s/.prepared", target);

    struct stat source_status;
    struct stat marker_status;
    return stat(source, &source_status) == 0 &&
           stat(marker, &marker_status) == 0 &&
           marker_status.st_mtime >= source_status.st_mtime;
}

static void mark_archive_prepared(const char *target)
{
    char marker[384];
    snprintf(marker, sizeof(marker), "%s/.prepared", target);
    FILE *file = fopen(marker, "wb");
    if (file != NULL) {
        fputs("prepared\n", file);
        fclose(file);
    }
}

static bool prepare_archive(
    const char *source,
    const char *target,
    bool metadata_only,
    const char *display_name)
{
    if (mkdir(target, 0775) != 0 && errno != EEXIST) {
        return false;
    }

    if (!metadata_only && archive_is_prepared(source, target)) {
        ESP_LOGI(TAG, "Using prepared %s", display_name);
        return true;
    }

    if (!extract_archive(source, target, metadata_only)) {
        ESP_LOGW(TAG, "Could not prepare %s", display_name);
        return false;
    }

    if (!metadata_only) {
        mark_archive_prepared(target);
        ESP_LOGI(TAG, "Prepared %s", display_name);
    }
    return true;
}

bool dice_set_archive_prepare_catalog(void)
{
    if (mkdir(CACHE_ROOT, 0775) != 0 && errno != EEXIST) {
        return false;
    }

    DIR *dir = opendir(SETS_ROOT);
    if (dir == NULL) {
        return true;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.' ||
            !ends_with_set(entry->d_name)) {
            continue;
        }

        char source[320];
        char target[320];
        char base[128];
        snprintf(source, sizeof(source), "%s/%s", SETS_ROOT, entry->d_name);
        snprintf(base, sizeof(base), "%s", entry->d_name);
        base[strlen(base) - 4] = '\0';
        snprintf(target, sizeof(target), "%s/%s", CACHE_ROOT, base);

        if (!archive_is_prepared(source, target)) {
            prepare_archive(source, target, true, entry->d_name);
        }
    }

    closedir(dir);
    return true;
}

bool dice_set_archive_prepare_set(const char *folder_path)
{
    if (folder_path == NULL || folder_path[0] == '\0') {
        return false;
    }

    size_t cache_root_length = strlen(CACHE_ROOT);
    if (strncmp(folder_path, CACHE_ROOT, cache_root_length) != 0 ||
        folder_path[cache_root_length] != '/') {
        return path_is_directory(folder_path);
    }

    const char *base = folder_path + cache_root_length + 1;
    if (base[0] == '\0' || strchr(base, '/') != NULL) {
        return false;
    }

    char source[320];
    snprintf(source, sizeof(source), "%s/%s.set", SETS_ROOT, base);
    return prepare_archive(source, folder_path, false, base);
}
