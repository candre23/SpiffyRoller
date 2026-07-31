#include "dice_storage.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "wear_levelling.h"

#define STORAGE_PARTITION_LABEL "storage"
#define STORAGE_MAX_FILES 8
#define STORAGE_ALLOCATION_UNIT 4096

static const char *TAG = "dice_storage";
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;
static bool s_mounted;

static bool ensure_directory(const char *path)
{
    if (mkdir(path, 0775) == 0 || errno == EEXIST) {
        return true;
    }

    ESP_LOGE(
        TAG,
        "Could not create %s: errno=%d",
        path,
        errno);
    return false;
}

static void create_readme_if_missing(void)
{
    static const char *path =
        DICE_STORAGE_MOUNT_POINT "/README.TXT";

    FILE *existing = fopen(path, "rb");
    if (existing != NULL) {
        fclose(existing);
        return;
    }

    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        ESP_LOGW(TAG, "Could not create README.TXT");
        return;
    }

    static const char contents[] =
        "ESP32 Dice Roller Storage\r\n"
        "==========================\r\n"
        "\r\n"
        "audio/dice_roll.wav\r\n"
        "  Optional custom rolling sound.\r\n"
        "  Required format: mono, 16-bit PCM, 16000 Hz WAV.\r\n"
        "  If missing or invalid, the roller generates a fallback sound.\r\n"
        "\r\n"
        "templates/\r\n"
        "  Put each custom dice set in its own folder.\r\n"
        "  Each folder must contain a valid set.json manifest.\r\n"
        "\r\n"
        "icons/\r\n"
        "  Reserved for user-provided graphics.\r\n";

    fwrite(contents, 1, sizeof(contents) - 1, file);
    fclose(file);
}

bool dice_storage_mount(void)
{
    if (s_mounted) {
        return true;
    }

    const esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = STORAGE_MAX_FILES,
        .allocation_unit_size = STORAGE_ALLOCATION_UNIT,
        .disk_status_check_enable = false,
        .use_one_fat = false,
    };

    esp_err_t result = esp_vfs_fat_spiflash_mount_rw_wl(
        DICE_STORAGE_MOUNT_POINT,
        STORAGE_PARTITION_LABEL,
        &mount_config,
        &s_wl_handle);

    if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Could not mount FAT storage partition: %s",
            esp_err_to_name(result));
        return false;
    }

    s_mounted = true;

    bool directories_ok =
        ensure_directory(DICE_STORAGE_MOUNT_POINT "/audio") &&
        ensure_directory(DICE_STORAGE_MOUNT_POINT "/templates") &&
        ensure_directory(DICE_STORAGE_MOUNT_POINT "/icons");

    create_readme_if_missing();

    ESP_LOGI(
        TAG,
        "Storage mounted at %s",
        DICE_STORAGE_MOUNT_POINT);

    return directories_ok;
}

bool dice_storage_is_mounted(void)
{
    return s_mounted;
}

bool dice_storage_get_usage(
    size_t *total_bytes,
    size_t *used_bytes)
{
    if (total_bytes == NULL || used_bytes == NULL || !s_mounted) {
        return false;
    }

    uint64_t total = 0;
    uint64_t free = 0;
    esp_err_t result = esp_vfs_fat_info(
        DICE_STORAGE_MOUNT_POINT,
        &total,
        &free);

    if (result != ESP_OK || free > total) {
        ESP_LOGW(
            TAG,
            "Could not read storage usage: %s",
            esp_err_to_name(result));
        return false;
    }

    *total_bytes = (size_t)total;
    *used_bytes = (size_t)(total - free);
    return true;
}

bool dice_storage_unmount(void)
{
    if (!s_mounted) {
        return true;
    }

    esp_err_t result =
        esp_vfs_fat_spiflash_unmount_rw_wl(
            DICE_STORAGE_MOUNT_POINT,
            s_wl_handle);

    if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Could not unmount storage: %s",
            esp_err_to_name(result));
        return false;
    }

    s_mounted = false;
    s_wl_handle = WL_INVALID_HANDLE;
    ESP_LOGI(TAG, "Storage unmounted");
    return true;
}
