#include "dice_battery.h"

#include <stddef.h>
#include <stdint.h>

#include "bsp/esp-bsp.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define AXP2101_I2C_ADDRESS          0x34
#define AXP2101_BATTERY_VOLTAGE_REG  0x34
#define AXP2101_STATUS_REG           0x00
#define AXP2101_VBUS_GOOD_MASK       (1U << 5)
#define BATTERY_UPDATE_PERIOD_MS     5000
#define BATTERY_INITIAL_DELAY_MS     700
#define BATTERY_VALID_MIN_MV         2800
#define BATTERY_VALID_MAX_MV         4600

static const char *TAG = "dice_battery";

typedef struct {
    uint16_t millivolts;
    uint8_t percent;
} discharge_curve_point_t;

/*
 * Approximate single-cell LiPo open-circuit discharge curve.
 * The voltage remains visible in the UI because load, charging, temperature,
 * and battery age can shift the apparent state of charge.
 */
static const discharge_curve_point_t s_curve[] = {
    {4200, 100},
    {4150, 95},
    {4100, 90},
    {4050, 85},
    {4000, 80},
    {3950, 75},
    {3900, 70},
    {3850, 64},
    {3800, 57},
    {3750, 50},
    {3710, 42},
    {3680, 34},
    {3650, 27},
    {3620, 20},
    {3580, 13},
    {3500, 6},
    {3300, 0},
};

static dice_battery_update_callback_t s_callback;
static void *s_callback_context;

static uint8_t estimate_percent(uint16_t voltage_mv)
{
    if (voltage_mv >= s_curve[0].millivolts) {
        return 100;
    }

    const size_t curve_count =
        sizeof(s_curve) / sizeof(s_curve[0]);

    if (voltage_mv <= s_curve[curve_count - 1].millivolts) {
        return 0;
    }

    for (size_t index = 1; index < curve_count; ++index) {
        const discharge_curve_point_t *high = &s_curve[index - 1];
        const discharge_curve_point_t *low = &s_curve[index];

        if (voltage_mv <= high->millivolts &&
            voltage_mv >= low->millivolts) {
            uint16_t voltage_span =
                high->millivolts - low->millivolts;
            uint16_t voltage_above_low =
                voltage_mv - low->millivolts;
            uint8_t percent_span =
                high->percent - low->percent;

            return (uint8_t)(
                low->percent +
                ((uint32_t)voltage_above_low * percent_span +
                 voltage_span / 2) /
                    voltage_span);
        }
    }

    return 0;
}

static esp_err_t read_registers(
    i2c_master_dev_handle_t device,
    uint8_t start_register,
    uint8_t *data,
    size_t length)
{
    return i2c_master_transmit_receive(
        device,
        &start_register,
        1,
        data,
        length,
        100);
}

static bool read_battery_voltage(
    i2c_master_dev_handle_t device,
    uint16_t *voltage_mv)
{
    uint8_t raw[2] = {0};

    if (read_registers(
            device,
            AXP2101_BATTERY_VOLTAGE_REG,
            raw,
            sizeof(raw)) != ESP_OK) {
        return false;
    }

    /*
     * AXP2101 VBAT ADC is a 14-bit value with 1 mV per count.
     * The high register contributes its low six bits.
     */
    uint16_t value =
        (uint16_t)(((raw[0] & 0x3fU) << 8) | raw[1]);

    if (value < BATTERY_VALID_MIN_MV ||
        value > BATTERY_VALID_MAX_MV) {
        return false;
    }

    *voltage_mv = value;
    return true;
}

static bool read_usb_power_present(
    i2c_master_dev_handle_t device,
    bool *usb_power_present)
{
    uint8_t status = 0;

    if (read_registers(
            device,
            AXP2101_STATUS_REG,
            &status,
            1) != ESP_OK) {
        return false;
    }

    *usb_power_present =
        (status & AXP2101_VBUS_GOOD_MASK) != 0;
    return true;
}

static void battery_task(void *argument)
{
    (void)argument;

    vTaskDelay(pdMS_TO_TICKS(BATTERY_INITIAL_DELAY_MS));

    esp_err_t result = bsp_i2c_init();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "BSP I2C initialization failed");
        vTaskDelete(NULL);
        return;
    }

    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (bus == NULL) {
        ESP_LOGE(TAG, "BSP I2C handle unavailable");
        vTaskDelete(NULL);
        return;
    }

    i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AXP2101_I2C_ADDRESS,
        .scl_speed_hz = 400000,
    };

    i2c_master_dev_handle_t device = NULL;
    result = i2c_master_bus_add_device(
        bus,
        &device_config,
        &device);

    if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Could not attach AXP2101 I2C device: %s",
            esp_err_to_name(result));
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "AXP2101 battery monitor ready");

    while (true) {
        uint16_t voltage_mv = 0;
        bool available =
            read_battery_voltage(device, &voltage_mv);
        uint8_t percent =
            available ? estimate_percent(voltage_mv) : 0;
        bool usb_power_present = false;
        read_usb_power_present(
            device,
            &usb_power_present);

        if (s_callback != NULL) {
            s_callback(
                available,
                voltage_mv,
                percent,
                usb_power_present,
                s_callback_context);
        }

        if (available) {
            ESP_LOGI(
                TAG,
                "Battery: %u mV, estimated %u%%",
                (unsigned)voltage_mv,
                (unsigned)percent);
        } else {
            ESP_LOGW(TAG, "Battery voltage unavailable");
        }

        vTaskDelay(pdMS_TO_TICKS(BATTERY_UPDATE_PERIOD_MS));
    }
}

bool dice_battery_start(
    dice_battery_update_callback_t callback,
    void *context)
{
    s_callback = callback;
    s_callback_context = context;

    return xTaskCreate(
               battery_task,
               "dice_battery",
               4096,
               NULL,
               4,
               NULL) == pdPASS;
}
