#include "app_controller.h"
#include "dice_transfer.h"
#include "dice_display_reset.h"
#include "dice_ui.h"

#include <stdlib.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BOOT_GPIO GPIO_NUM_0
#define LONG_MS 3000

static const char *TAG = "spiffy_roller";

static void button_task(void *argument)
{
    (void)argument;

    gpio_config_t config = {
        .pin_bit_mask = 1ULL << BOOT_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&config));

    bool down = false;
    bool long_sent = false;
    int64_t press_start = 0;

    while (true) {
        bool pressed = gpio_get_level(BOOT_GPIO) == 0;
        int64_t now = esp_timer_get_time();

        if (pressed && !down) {
            down = true;
            long_sent = false;
            press_start = now;
        }

        if (pressed && down && !long_sent &&
            now - press_start >= LONG_MS * 1000LL) {
            long_sent = true;
            app_controller_notify_boot(true);
        }

        if (!pressed && down) {
            down = false;
            if (!long_sent) {
                app_controller_notify_boot(false);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Spiffy Roller v1.0");

    if (!dice_transfer_initialize_flag_storage()) {
        ESP_LOGE(TAG, "NVS initialization failed");
        abort();
    }

    if (dice_transfer_was_requested()) {
        dice_transfer_run();
        return;
    }

    /*
     * A warm reset does not remove power from the external AMOLED controller
     * or all board peripherals. Give them time to settle before the BSP sends
     * the normal initialization sequence.
     */
    vTaskDelay(pdMS_TO_TICKS(750));

    if (!dice_display_hardware_reset()) {
        ESP_LOGW(TAG, "AMOLED hardware reset failed");
    }
    if (!dice_ui_startup_begin("v1.0")) {
        ESP_LOGE(TAG, "Startup display initialization failed");
        abort();
    }

    if (!app_controller_start()) {
        ESP_LOGE(TAG, "Application startup failed");
        abort();
    }

    xTaskCreate(
        button_task,
        "boot_button",
        3072,
        NULL,
        5,
        NULL);

    ESP_LOGI(
        TAG,
        "Touch selector, swipe to adjust, shake to roll");
}
