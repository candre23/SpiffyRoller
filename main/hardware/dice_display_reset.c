#include "dice_display_reset.h"

#include <stdint.h>

#include "bsp/esp-bsp.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TCA9554_I2C_ADDRESS 0x20
#define TCA9554_OUTPUT_REGISTER 0x01
#define TCA9554_CONFIG_REGISTER 0x03

/*
 * On the ESP32-S3-Touch-AMOLED-1.8 V2, EXIO1 is the AMOLED reset line.
 * It is active low.
 */
#define AMOLED_RESET_MASK (1U << 1)

#define RESET_LOW_MS 80
#define RESET_RECOVERY_MS 180
#define RESET_ATTEMPTS 5
#define RESET_RETRY_DELAY_MS 100

static const char *TAG = "display_reset";

static esp_err_t read_register(
    i2c_master_dev_handle_t device,
    uint8_t register_address,
    uint8_t *value)
{
    return i2c_master_transmit_receive(
        device,
        &register_address,
        1,
        value,
        1,
        100);
}

static esp_err_t write_register(
    i2c_master_dev_handle_t device,
    uint8_t register_address,
    uint8_t value)
{
    uint8_t data[2] = {
        register_address,
        value,
    };

    return i2c_master_transmit(
        device,
        data,
        sizeof(data),
        100);
}

bool dice_display_hardware_reset(void)
{
    esp_err_t result = bsp_i2c_init();
    if (result != ESP_OK &&
        result != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(
            TAG,
            "BSP I2C initialization failed: %s",
            esp_err_to_name(result));
        return false;
    }

    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (bus == NULL) {
        ESP_LOGE(TAG, "BSP I2C handle unavailable");
        return false;
    }

    for (unsigned int attempt = 1;
         attempt <= RESET_ATTEMPTS;
         ++attempt) {
        /*
         * A browser flash ends with a warm reset. The external expander can
         * briefly remain unresponsive while the ESP32 and I2C controller are
         * already running, so create a fresh device handle for each attempt.
         */
        i2c_device_config_t device_config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = TCA9554_I2C_ADDRESS,
            .scl_speed_hz = attempt == RESET_ATTEMPTS ? 100000 : 400000,
        };

        i2c_master_dev_handle_t device = NULL;
        result = i2c_master_bus_add_device(
            bus,
            &device_config,
            &device);

        if (result != ESP_OK) {
            ESP_LOGW(
                TAG,
                "TCA9554 attach attempt %u/%u failed: %s",
                attempt,
                RESET_ATTEMPTS,
                esp_err_to_name(result));
        } else {
            uint8_t output = 0;
            uint8_t config = 0;

            result = read_register(
                device,
                TCA9554_OUTPUT_REGISTER,
                &output);

            if (result == ESP_OK) {
                result = read_register(
                    device,
                    TCA9554_CONFIG_REGISTER,
                    &config);
            }

            if (result == ESP_OK) {
                /* Configure EXIO1 as an output, preserving other pins. */
                const uint8_t reset_config =
                    (uint8_t)(config & ~AMOLED_RESET_MASK);

                result = write_register(
                    device,
                    TCA9554_CONFIG_REGISTER,
                    reset_config);

                if (result == ESP_OK) {
                    result = write_register(
                        device,
                        TCA9554_OUTPUT_REGISTER,
                        (uint8_t)(output & ~AMOLED_RESET_MASK));
                }

                if (result == ESP_OK) {
                    vTaskDelay(pdMS_TO_TICKS(RESET_LOW_MS));
                    result = write_register(
                        device,
                        TCA9554_OUTPUT_REGISTER,
                        (uint8_t)(output | AMOLED_RESET_MASK));
                }
            }

            esp_err_t remove_result =
                i2c_master_bus_rm_device(device);
            if (remove_result != ESP_OK) {
                ESP_LOGW(
                    TAG,
                    "Could not remove temporary TCA9554 handle: %s",
                    esp_err_to_name(remove_result));
            }

            if (result == ESP_OK) {
                vTaskDelay(pdMS_TO_TICKS(RESET_RECOVERY_MS));
                ESP_LOGI(
                    TAG,
                    "AMOLED hardware reset completed on attempt %u",
                    attempt);
                return true;
            }

            ESP_LOGW(
                TAG,
                "TCA9554 reset attempt %u/%u failed: %s",
                attempt,
                RESET_ATTEMPTS,
                esp_err_to_name(result));
        }

        if (attempt < RESET_ATTEMPTS) {
            vTaskDelay(pdMS_TO_TICKS(RESET_RETRY_DELAY_MS));
        }
    }

    ESP_LOGE(
        TAG,
        "AMOLED hardware reset failed after %u attempts",
        RESET_ATTEMPTS);
    return false;
}
