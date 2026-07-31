#include "dice_set_catalog.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "cJSON.h"
#include "dice_storage.h"
#include "dice_set_archive.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#define SETS_ROOT DICE_STORAGE_MOUNT_POINT "/templates"
#define SET_CACHE_ROOT DICE_STORAGE_MOUNT_POINT "/set_cache"
#define SET_MANIFEST_NAME "set.json"
#define MAX_MANIFEST_BYTES 65536

static const char *TAG = "dice_sets";
static dice_set_definition_t *s_sets;
static size_t s_set_count;

static bool ensure_catalog_storage(void)
{
    if (s_sets != NULL) {
        memset(
            s_sets,
            0,
            sizeof(*s_sets) * DICE_SET_MAX_SETS);
        return true;
    }

    size_t bytes =
        sizeof(*s_sets) * DICE_SET_MAX_SETS;

    bool allocated_in_psram = true;

    s_sets = heap_caps_calloc(
        DICE_SET_MAX_SETS,
        sizeof(*s_sets),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (s_sets == NULL) {
        allocated_in_psram = false;

        ESP_LOGW(
            TAG,
            "PSRAM catalog allocation failed; "
            "trying general heap");

        s_sets = heap_caps_calloc(
            DICE_SET_MAX_SETS,
            sizeof(*s_sets),
            MALLOC_CAP_8BIT);
    }

    if (s_sets == NULL) {
        ESP_LOGE(
            TAG,
            "Could not allocate %u bytes for set catalog",
            (unsigned)bytes);
        return false;
    }

    ESP_LOGI(
        TAG,
        "Allocated %u-byte set catalog in %s",
        (unsigned)bytes,
        allocated_in_psram
            ? "PSRAM"
            : "general heap");

    return true;
}

static void copy_string(
    char *destination,
    size_t destination_size,
    const char *source)
{
    if (destination == NULL || destination_size == 0) {
        return;
    }

    if (source == NULL) {
        destination[0] = '\0';
        return;
    }

    snprintf(destination, destination_size, "%s", source);
}

static bool valid_identifier(const char *text)
{
    if (text == NULL || text[0] == '\0') {
        return false;
    }

    for (const unsigned char *cursor =
             (const unsigned char *)text;
         *cursor != '\0';
         ++cursor) {
        if (!(isalnum(*cursor) ||
              *cursor == '_' ||
              *cursor == '-')) {
            return false;
        }
    }

    return true;
}

static bool parse_color(
    const cJSON *item,
    uint32_t *color)
{
    if (!cJSON_IsString(item) ||
        item->valuestring == NULL ||
        item->valuestring[0] != '#' ||
        strlen(item->valuestring) != 7) {
        return false;
    }

    char *end = NULL;
    unsigned long value = strtoul(
        item->valuestring + 1,
        &end,
        16);

    if (end == NULL || *end != '\0' ||
        value > 0xffffffUL) {
        return false;
    }

    *color = (uint32_t)value;
    return true;
}

static bool parse_optional_color(
    const cJSON *object,
    const char *key,
    uint32_t *color,
    bool *present)
{
    const cJSON *item =
        cJSON_GetObjectItemCaseSensitive(object, key);

    if (item == NULL) {
        *present = false;
        return true;
    }

    if (!parse_color(item, color)) {
        return false;
    }

    *present = true;
    return true;
}

static bool safe_relative_asset_path(const char *path)
{
    if (path == NULL ||
        path[0] == '\0' ||
        path[0] == '/' ||
        path[0] == '\\' ||
        strstr(path, "..") != NULL ||
        strchr(path, ':') != NULL) {
        return false;
    }

    return true;
}

static bool parse_symbol_count(
    const cJSON *symbols,
    const char *key,
    uint8_t *value)
{
    const cJSON *item =
        cJSON_GetObjectItemCaseSensitive(
            symbols,
            key);

    if (item == NULL) {
        *value = 0;
        return true;
    }

    if (!cJSON_IsNumber(item) ||
        item->valueint < 0 ||
        item->valueint > 4) {
        return false;
    }

    *value = (uint8_t)item->valueint;
    return true;
}

static bool parse_face_symbols(
    const cJSON *json,
    dice_set_face_t *face)
{
    const cJSON *symbols =
        cJSON_GetObjectItemCaseSensitive(
            json,
            "symbols");

    if (symbols == NULL) {
        return true;
    }

    if (!cJSON_IsObject(symbols) ||
        !parse_symbol_count(
            symbols,
            "success",
            &face->success_count) ||
        !parse_symbol_count(
            symbols,
            "advantage",
            &face->advantage_count) ||
        !parse_symbol_count(
            symbols,
            "triumph",
            &face->triumph_count) ||
        !parse_symbol_count(
            symbols,
            "failure",
            &face->failure_count) ||
        !parse_symbol_count(
            symbols,
            "threat",
            &face->threat_count) ||
        !parse_symbol_count(
            symbols,
            "despair",
            &face->despair_count)) {
        return false;
    }

    face->has_symbol_values = true;
    return true;
}

static bool parse_face_display_mode(
    const cJSON *json,
    dice_face_display_mode_t *display_mode,
    bool *explicit_mode)
{
    const cJSON *mode =
        cJSON_GetObjectItemCaseSensitive(
            json,
            "display_mode");

    *explicit_mode = false;
    *display_mode = DICE_FACE_DISPLAY_BLANK;

    if (mode == NULL) {
        return true;
    }

    if (!cJSON_IsString(mode) || mode->valuestring == NULL) {
        return false;
    }

    *explicit_mode = true;
    if (strcmp(mode->valuestring, "label") == 0) {
        *display_mode = DICE_FACE_DISPLAY_LABEL;
    } else if (strcmp(mode->valuestring, "value") == 0) {
        *display_mode = DICE_FACE_DISPLAY_VALUE;
    } else if (strcmp(mode->valuestring, "image") == 0) {
        *display_mode = DICE_FACE_DISPLAY_IMAGE;
    } else if (strcmp(mode->valuestring, "blank") == 0) {
        *display_mode = DICE_FACE_DISPLAY_BLANK;
    } else {
        return false;
    }

    return true;
}

static bool parse_face(
    const cJSON *json,
    const dice_set_die_t *die,
    dice_set_face_t *face)
{
    memset(face, 0, sizeof(*face));
    face->body_color = die->body_color;
    face->ink_color = die->ink_color;
    face->display_mode = DICE_FACE_DISPLAY_BLANK;

    const cJSON *value =
        cJSON_GetObjectItemCaseSensitive(json, "value");
    if (value != NULL) {
        if (!cJSON_IsNumber(value)) {
            return false;
        }

        face->numeric_value = value->valueint;
        face->has_numeric_value = true;
    }

    const cJSON *label =
        cJSON_GetObjectItemCaseSensitive(json, "label");
    if (label != NULL) {
        if (!cJSON_IsString(label) ||
            label->valuestring == NULL ||
            strlen(label->valuestring) >
                DICE_SET_LABEL_MAX) {
            return false;
        }

        copy_string(
            face->label,
            sizeof(face->label),
            label->valuestring);
        face->art_mode = DICE_FACE_ART_TEXT;
    }

    bool explicit_display_mode = false;
    if (!parse_face_display_mode(
            json,
            &face->display_mode,
            &explicit_display_mode)) {
        return false;
    }

    const cJSON *image =
        cJSON_GetObjectItemCaseSensitive(json, "image");
    const cJSON *image_mode =
        cJSON_GetObjectItemCaseSensitive(
            json,
            "image_mode");

    if (image != NULL) {
        if (!cJSON_IsString(image) ||
            image->valuestring == NULL ||
            strlen(image->valuestring) >
                DICE_SET_PATH_MAX ||
            !safe_relative_asset_path(
                image->valuestring)) {
            return false;
        }

        if (image_mode != NULL) {
            if (!cJSON_IsString(image_mode) ||
                image_mode->valuestring == NULL) {
                return false;
            }

            if (strcmp(
                    image_mode->valuestring,
                    "mask") == 0) {
                face->art_mode = DICE_FACE_ART_MASK;
            } else if (strcmp(
                           image_mode->valuestring,
                           "indexed") == 0) {
                face->art_mode = DICE_FACE_ART_INDEXED;
            } else {
                return false;
            }
        } else {
            face->art_mode = DICE_FACE_ART_MASK;
        }

        copy_string(
            face->image_path,
            sizeof(face->image_path),
            image->valuestring);
    } else if (image_mode != NULL) {
        return false;
    }

    if (!parse_face_symbols(
            json,
            face)) {
        return false;
    }

    if (!parse_optional_color(
            json,
            "body_color",
            &face->body_color,
            &face->has_body_color_override) ||
        !parse_optional_color(
            json,
            "ink_color",
            &face->ink_color,
            &face->has_ink_color_override)) {
        return false;
    }

    if (!explicit_display_mode) {
        if (face->image_path[0] != '\0') {
            face->display_mode = DICE_FACE_DISPLAY_IMAGE;
        } else if (face->label[0] != '\0') {
            face->display_mode = DICE_FACE_DISPLAY_LABEL;
        } else if (face->has_numeric_value) {
            face->display_mode = DICE_FACE_DISPLAY_VALUE;
        } else {
            face->display_mode = DICE_FACE_DISPLAY_BLANK;
        }
    }

    return true;
}

static bool parse_die(

    const cJSON *json,
    dice_set_die_t *die)
{
    memset(die, 0, sizeof(*die));

    const cJSON *id =
        cJSON_GetObjectItemCaseSensitive(json, "id");
    const cJSON *name =
        cJSON_GetObjectItemCaseSensitive(json, "name");
    const cJSON *shape =
        cJSON_GetObjectItemCaseSensitive(json, "shape");
    const cJSON *sides =
        cJSON_GetObjectItemCaseSensitive(json, "sides");
    const cJSON *body_color =
        cJSON_GetObjectItemCaseSensitive(
            json,
            "body_color");
    const cJSON *ink_color =
        cJSON_GetObjectItemCaseSensitive(
            json,
            "ink_color");
    const cJSON *faces =
        cJSON_GetObjectItemCaseSensitive(json, "faces");

    if (!cJSON_IsString(id) ||
        !valid_identifier(id->valuestring) ||
        strlen(id->valuestring) > DICE_SET_ID_MAX ||
        !cJSON_IsString(name) ||
        name->valuestring == NULL ||
        strlen(name->valuestring) > DICE_SET_NAME_MAX ||
        !cJSON_IsString(shape) ||
        shape->valuestring == NULL ||
        strlen(shape->valuestring) > DICE_SET_SHAPE_MAX ||
        !cJSON_IsNumber(sides) ||
        sides->valueint < 2 ||
        sides->valueint > DICE_SET_MAX_FACES ||
        !parse_color(body_color, &die->body_color) ||
        !parse_color(ink_color, &die->ink_color) ||
        !cJSON_IsArray(faces) ||
        cJSON_GetArraySize(faces) != sides->valueint) {
        return false;
    }

    copy_string(die->id, sizeof(die->id), id->valuestring);
    copy_string(
        die->name,
        sizeof(die->name),
        name->valuestring);
    copy_string(
        die->shape,
        sizeof(die->shape),
        shape->valuestring);
    die->side_count = (uint16_t)sides->valueint;
    die->face_count = (size_t)sides->valueint;

    for (size_t index = 0;
         index < die->face_count;
         ++index) {
        const cJSON *face =
            cJSON_GetArrayItem(faces, (int)index);

        if (!cJSON_IsObject(face) ||
            !parse_face(
                face,
                die,
                &die->faces[index])) {
            return false;
        }
    }

    return true;
}

static char *read_manifest(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    long length = ftell(file);
    if (length <= 0 ||
        length > MAX_MANIFEST_BYTES ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    char *buffer = malloc((size_t)length + 1);
    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }

    size_t bytes_read =
        fread(buffer, 1, (size_t)length, file);
    fclose(file);

    if (bytes_read != (size_t)length) {
        free(buffer);
        return NULL;
    }

    buffer[length] = '\0';
    return buffer;
}

static bool asset_file_exists(
    const char *folder_path,
    const char *relative_path)
{
    if (!safe_relative_asset_path(relative_path)) {
        return false;
    }

    char full_path[DICE_SET_PATH_MAX * 2 + 4];
    int written = snprintf(
        full_path,
        sizeof(full_path),
        "%s/%s",
        folder_path,
        relative_path);

    if (written < 0 ||
        written >= (int)sizeof(full_path)) {
        return false;
    }

    struct stat status;
    return stat(full_path, &status) == 0 &&
           S_ISREG(status.st_mode);
}

static bool parse_manifest(
    const char *folder_path,
    dice_set_definition_t *set)
{
    char manifest_path[DICE_SET_PATH_MAX + 32];
    snprintf(
        manifest_path,
        sizeof(manifest_path),
        "%s/%s",
        folder_path,
        SET_MANIFEST_NAME);

    char *text = read_manifest(manifest_path);
    if (text == NULL) {
        return false;
    }

    cJSON *root = cJSON_Parse(text);
    free(text);

    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return false;
    }

    memset(set, 0, sizeof(*set));

    const cJSON *format =
        cJSON_GetObjectItemCaseSensitive(root, "format");
    const cJSON *format_version =
        cJSON_GetObjectItemCaseSensitive(
            root,
            "format_version");
    const cJSON *id =
        cJSON_GetObjectItemCaseSensitive(root, "id");
    const cJSON *name =
        cJSON_GetObjectItemCaseSensitive(root, "name");
    const cJSON *dice =
        cJSON_GetObjectItemCaseSensitive(root, "dice");
    const cJSON *rules =
        cJSON_GetObjectItemCaseSensitive(root, "rules");

    bool valid =
        cJSON_IsString(format) &&
        format->valuestring != NULL &&
        strcmp(
            format->valuestring,
            "universal-dice-set") == 0 &&
        cJSON_IsNumber(format_version) &&
        format_version->valueint == 1 &&
        cJSON_IsString(id) &&
        valid_identifier(id->valuestring) &&
        strlen(id->valuestring) <= DICE_SET_ID_MAX &&
        cJSON_IsString(name) &&
        name->valuestring != NULL &&
        strlen(name->valuestring) <= DICE_SET_NAME_MAX &&
        cJSON_IsArray(dice);

    int die_count =
        valid ? cJSON_GetArraySize(dice) : 0;

    if (die_count < 1 ||
        die_count > DICE_SET_MAX_DICE) {
        valid = false;
    }

    bool archive_metadata =
        strncmp(
            folder_path,
            SET_CACHE_ROOT,
            strlen(SET_CACHE_ROOT)) == 0;

    if (rules != NULL &&
        (!cJSON_IsString(rules) ||
         rules->valuestring == NULL ||
         strlen(rules->valuestring) > DICE_SET_PATH_MAX ||
         (!archive_metadata &&
          !asset_file_exists(folder_path, rules->valuestring)))) {
        valid = false;
    }

    if (valid) {
        copy_string(set->id, sizeof(set->id), id->valuestring);
        copy_string(
            set->name,
            sizeof(set->name),
            name->valuestring);
        copy_string(
            set->folder_path,
            sizeof(set->folder_path),
            folder_path);
        set->format_version =
            (uint16_t)format_version->valueint;
        set->die_count = (size_t)die_count;

        if (rules != NULL) {
            copy_string(
                set->rules_path,
                sizeof(set->rules_path),
                rules->valuestring);
        }

        for (int index = 0;
             index < die_count;
             ++index) {
            const cJSON *die =
                cJSON_GetArrayItem(dice, index);

            if (!cJSON_IsObject(die) ||
                !parse_die(
                    die,
                    &set->dice[index])) {
                valid = false;
                break;
            }

            for (int previous = 0;
                 previous < index;
                 ++previous) {
                if (strcmp(
                        set->dice[previous].id,
                        set->dice[index].id) == 0) {
                    valid = false;
                    break;
                }
            }

            if (!valid) {
                break;
            }

            for (size_t face_index = 0;
                 face_index <
                     set->dice[index].face_count;
                 ++face_index) {
                const dice_set_face_t *face =
                    &set->dice[index].faces[face_index];

                if (!archive_metadata &&
                    face->image_path[0] != '\0' &&
                    !asset_file_exists(
                        folder_path,
                        face->image_path)) {
                    valid = false;
                    break;
                }
            }

            if (!valid) {
                break;
            }
        }
    }

    set->valid = valid;
    cJSON_Delete(root);
    return valid;
}

static void add_standard_set(void)
{
    dice_set_definition_t *standard =
        &s_sets[s_set_count++];

    memset(standard, 0, sizeof(*standard));
    copy_string(
        standard->id,
        sizeof(standard->id),
        "standard");
    copy_string(
        standard->name,
        sizeof(standard->name),
        "Standard");
    standard->format_version = 1;
    standard->built_in = true;
    standard->valid = true;
}

static bool is_directory(const char *path)
{
    struct stat status;
    return stat(path, &status) == 0 &&
           S_ISDIR(status.st_mode);
}

static int compare_sets_by_name(
    const void *left,
    const void *right)
{
    const dice_set_definition_t *left_set = left;
    const dice_set_definition_t *right_set = right;
    return strcasecmp(
        left_set->name,
        right_set->name);
}

static void scan_set_directory(const char *root)
{
    DIR *directory = opendir(root);
    if (directory == NULL) {
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL &&
           s_set_count < DICE_SET_MAX_SETS) {
        if (entry->d_name[0] == '.') {
            continue;
        }

        char folder_path[DICE_SET_PATH_MAX + 1];
        int written = snprintf(
            folder_path,
            sizeof(folder_path),
            "%s/%s",
            root,
            entry->d_name);

        if (written < 0 ||
            written >= (int)sizeof(folder_path) ||
            !is_directory(folder_path)) {
            continue;
        }

        dice_set_definition_t *candidate = &s_sets[s_set_count];
        if (!parse_manifest(folder_path, candidate)) {
            continue;
        }

        bool duplicate = false;
        for (size_t index = 0; index < s_set_count; ++index) {
            if (strcmp(s_sets[index].id, candidate->id) == 0) {
                duplicate = true;
                break;
            }
        }

        if (duplicate) {
            memset(candidate, 0, sizeof(*candidate));
            continue;
        }

        ++s_set_count;
    }

    closedir(directory);
}

bool dice_set_catalog_scan(void)
{
    if (!ensure_catalog_storage()) return false;
    s_set_count = 0;
    add_standard_set();
    if (!dice_storage_is_mounted()) return true;
    dice_set_archive_prepare_catalog();
    scan_set_directory(SETS_ROOT);
    scan_set_directory(SET_CACHE_ROOT);

    if (s_set_count > 2) {
        qsort(
            &s_sets[1],
            s_set_count - 1,
            sizeof(s_sets[0]),
            compare_sets_by_name);
    }
    ESP_LOGI(TAG, "Loaded %u dice sets", (unsigned)s_set_count);
    return true;
}

size_t dice_set_catalog_count(void)
{
    return s_set_count;
}

const dice_set_definition_t *dice_set_catalog_get(size_t index)
{
    if (s_sets == NULL || index >= s_set_count) {
        return NULL;
    }

    return &s_sets[index];
}

const dice_set_definition_t *dice_set_catalog_find(const char *id)
{
    if (s_sets == NULL || id == NULL) {
        return NULL;
    }

    for (size_t index = 0;
         index < s_set_count;
         ++index) {
        if (strcmp(s_sets[index].id, id) == 0) {
            return &s_sets[index];
        }
    }

    return NULL;
}

size_t dice_set_catalog_index_of(const char *id)
{
    if (s_sets != NULL && id != NULL) {
        for (size_t index = 0;
             index < s_set_count;
             ++index) {
            if (strcmp(s_sets[index].id, id) == 0) {
                return index;
            }
        }
    }

    return 0;
}
