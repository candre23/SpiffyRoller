#include "dice_power.h"

#include <stdint.h>

#include "bsp/esp-bsp.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define AXP2101_I2C_ADDRESS 0x34
#define AXP2101_COMMON_CONFIG_REG 0x10
#define AXP2101_SOFT_POWEROFF_MASK 0x01

#define AUTO_SHUTDOWN_US (5LL * 60LL * 1000000LL)
#define LOW_WARNING_MV 3550
#define LOW_SHUTDOWN_MV 3400
#define LOW_SHUTDOWN_SAMPLES 3

static const char *TAG = "dice_power";
static esp_pm_lock_handle_t s_high_performance_lock;
static int s_high_performance_users;
static int64_t s_last_activity_us;
static dice_power_shutdown_callback_t s_shutdown_callback;
static void *s_shutdown_context;
static bool s_usb_power_present;
static uint16_t s_filtered_voltage_mv;
static uint8_t s_low_voltage_samples;
static bool s_warning_sent;
static bool s_shutdown_started;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

static bool write_axp_register(uint8_t reg, uint8_t value)
{
    esp_err_t result = bsp_i2c_init();
    if (result != ESP_OK &&
        result != ESP_ERR_INVALID_STATE) {
        return false;
    }

    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (bus == NULL) {
        return false;
    }

    i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AXP2101_I2C_ADDRESS,
        .scl_speed_hz = 400000,
    };

    i2c_master_dev_handle_t device = NULL;
    result = i2c_master_bus_add_device(bus, &config, &device);
    if (result != ESP_OK) {
        return false;
    }

    uint8_t data[2] = {reg, value};
    result = i2c_master_transmit(
        device,
        data,
        sizeof(data),
        100);

    i2c_master_bus_rm_device(device);
    return result == ESP_OK;
}

static bool read_axp_register(uint8_t reg, uint8_t *value)
{
    esp_err_t result = bsp_i2c_init();
    if (result != ESP_OK &&
        result != ESP_ERR_INVALID_STATE) {
        return false;
    }

    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (bus == NULL) {
        return false;
    }

    i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AXP2101_I2C_ADDRESS,
        .scl_speed_hz = 400000,
    };

    i2c_master_dev_handle_t device = NULL;
    result = i2c_master_bus_add_device(bus, &config, &device);
    if (result != ESP_OK) {
        return false;
    }

    result = i2c_master_transmit_receive(
        device,
        &reg,
        1,
        value,
        1,
        100);

    i2c_master_bus_rm_device(device);
    return result == ESP_OK;
}

static void request_shutdown(const char *reason)
{
    if (s_shutdown_started) {
        return;
    }

    s_shutdown_started = true;
    ESP_LOGW(TAG, "Shutdown requested: %s", reason);

    if (s_shutdown_callback != NULL) {
        s_shutdown_callback(reason, s_shutdown_context);
    }

    vTaskDelay(pdMS_TO_TICKS(250));

    uint8_t common_config = 0;
    if (!read_axp_register(
            AXP2101_COMMON_CONFIG_REG,
            &common_config)) {
        ESP_LOGE(TAG, "Could not read AXP2101 REG10");
        s_shutdown_started = false;
        return;
    }

    if (!write_axp_register(
            AXP2101_COMMON_CONFIG_REG,
            common_config | AXP2101_SOFT_POWEROFF_MASK)) {
        ESP_LOGE(TAG, "AXP2101 soft power-off command failed");
        s_shutdown_started = false;
        return;
    }

    while (true) {
        vTaskDelay(portMAX_DELAY);
    }
}

static void power_task(void *argument)
{
    (void)argument;

    while (true) {
        if (!s_usb_power_present &&
            esp_timer_get_time() - s_last_activity_us >=
                AUTO_SHUTDOWN_US) {
            request_shutdown("Five-minute inactivity timeout");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

bool dice_power_start(
    dice_power_shutdown_callback_t shutdown_callback,
    void *context)
{
    s_shutdown_callback = shutdown_callback;
    s_shutdown_context = context;
    s_last_activity_us = esp_timer_get_time();

    esp_pm_config_t config = {
        .max_freq_mhz = 240,
        .min_freq_mhz = 40,
        .light_sleep_enable = false,
    };

    esp_err_t result = esp_pm_configure(&config);
    if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Dynamic frequency scaling setup failed: %s",
            esp_err_to_name(result));
        return false;
    }

    result = esp_pm_lock_create(
        ESP_PM_CPU_FREQ_MAX,
        0,
        "dice_active",
        &s_high_performance_lock);

    if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "CPU performance lock creation failed: %s",
            esp_err_to_name(result));
        return false;
    }

    ESP_LOGI(
        TAG,
        "Power management active: CPU 40-240 MHz, shutdown after 5 minutes");

    return xTaskCreate(
               power_task,
               "dice_power",
               4096,
               NULL,
               4,
               NULL) == pdPASS;
}

void dice_power_note_activity(void)
{
    s_last_activity_us = esp_timer_get_time();
}

void dice_power_high_performance_acquire(void)
{
    if (s_high_performance_lock == NULL) {
        return;
    }

    portENTER_CRITICAL(&s_lock);
    int previous = s_high_performance_users++;
    portEXIT_CRITICAL(&s_lock);

    if (previous == 0) {
        esp_pm_lock_acquire(s_high_performance_lock);
    }
}

void dice_power_high_performance_release(void)
{
    if (s_high_performance_lock == NULL) {
        return;
    }

    bool release = false;

    portENTER_CRITICAL(&s_lock);
    if (s_high_performance_users > 0) {
        --s_high_performance_users;
        release = s_high_performance_users == 0;
    }
    portEXIT_CRITICAL(&s_lock);

    if (release) {
        esp_pm_lock_release(s_high_performance_lock);
    }
}

void dice_power_update_battery(
    bool available,
    uint16_t voltage_mv,
    bool usb_power_present)
{
    s_usb_power_present = usb_power_present;

    if (!available || usb_power_present) {
        s_low_voltage_samples = 0;
        s_warning_sent = false;
        return;
    }

    if (s_filtered_voltage_mv == 0) {
        s_filtered_voltage_mv = voltage_mv;
    } else {
        s_filtered_voltage_mv =
            (uint16_t)(
                ((uint32_t)s_filtered_voltage_mv * 3U +
                 voltage_mv) /
                4U);
    }

    if (s_filtered_voltage_mv <= LOW_WARNING_MV &&
        !s_warning_sent) {
        ESP_LOGW(
            TAG,
            "Low battery warning: filtered voltage %u mV",
            (unsigned)s_filtered_voltage_mv);
        s_warning_sent = true;
    }

    if (s_filtered_voltage_mv <= LOW_SHUTDOWN_MV) {
        if (s_low_voltage_samples < UINT8_MAX) {
            ++s_low_voltage_samples;
        }

        if (s_low_voltage_samples >= LOW_SHUTDOWN_SAMPLES) {
            request_shutdown("Low battery protection");
        }
    } else {
        s_low_voltage_samples = 0;
    }
}
