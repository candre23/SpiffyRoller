#include "dice_state.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_log.h"
#include "nvs.h"

#define STATE_NAMESPACE "dice_state"
#define STATE_KEY "state"
#define STATE_MAGIC 0x44524345U
#define STATE_VERSION 3U

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    dice_saved_state_t state;
} stored_state_t;

static const char *TAG = "dice_state";

bool dice_state_load(dice_saved_state_t *state)
{
    if (state == NULL) {
        return false;
    }

    nvs_handle_t handle;
    esp_err_t result = nvs_open(
        STATE_NAMESPACE,
        NVS_READONLY,
        &handle);

    if (result != ESP_OK) {
        return false;
    }

    stored_state_t stored = {0};
    size_t size = sizeof(stored);

    result = nvs_get_blob(
        handle,
        STATE_KEY,
        &stored,
        &size);

    nvs_close(handle);

    if (result != ESP_OK ||
        size != sizeof(stored) ||
        stored.magic != STATE_MAGIC ||
        stored.version != STATE_VERSION ||
        stored.size != sizeof(dice_saved_state_t)) {
        return false;
    }

    *state = stored.state;
    ESP_LOGI(TAG, "Saved dice state restored");
    return true;
}

bool dice_state_save(const dice_saved_state_t *state)
{
    if (state == NULL) {
        return false;
    }

    stored_state_t stored = {
        .magic = STATE_MAGIC,
        .version = STATE_VERSION,
        .size = sizeof(dice_saved_state_t),
        .state = *state,
    };

    nvs_handle_t handle;
    esp_err_t result = nvs_open(
        STATE_NAMESPACE,
        NVS_READWRITE,
        &handle);

    if (result != ESP_OK) {
        return false;
    }

    result = nvs_set_blob(
        handle,
        STATE_KEY,
        &stored,
        sizeof(stored));

    if (result == ESP_OK) {
        result = nvs_commit(handle);
    }

    nvs_close(handle);

    if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Could not save state: %s",
            esp_err_to_name(result));
        return false;
    }

    ESP_LOGI(TAG, "Dice state saved");
    return true;
}
