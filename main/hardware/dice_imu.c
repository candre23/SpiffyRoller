#include "dice_imu.h"

#include <math.h>
#include <stdint.h>

/*
 * The Waveshare QMI8658 header defines M_PI even when the C library has
 * already defined it. Avoid the duplicate-definition warning, which ESP-IDF
 * promotes to a build error.
 */
#ifdef M_PI
#undef M_PI
#endif

#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "qmi8658.h"

#define SAMPLE_PERIOD_MS 20
#define SHAKE_HIT_WINDOW_US 500000LL
#define SHAKE_COOLDOWN_US 900000LL
#define SHAKE_REQUIRED_HITS 3

/*
 * This threshold is deliberately far below the roll threshold but high
 * enough to ignore normal sensor noise and microscopic table vibration.
 */
#define MOTION_WAKE_DELTA_MPS2 1.35f
#define MOTION_REQUIRED_SAMPLES 2
#define MOTION_COOLDOWN_US 1200000LL

static const char *TAG = "dice_imu";
static dice_imu_shake_callback_t s_shake_callback;
static dice_imu_motion_callback_t s_motion_callback;
static void *s_context;
static float s_shake_threshold = 12.0f;

static esp_err_t detect_address(
    i2c_master_bus_handle_t bus,
    uint8_t *address)
{
    static const uint8_t candidates[] = {0x6a, 0x6b};

    for (size_t index = 0;
         index < sizeof(candidates);
         ++index) {
        if (i2c_master_probe(
                bus,
                candidates[index],
                100) == ESP_OK) {
            *address = candidates[index];
            return ESP_OK;
        }
    }

    return ESP_ERR_NOT_FOUND;
}

static esp_err_t setup_imu(qmi8658_dev_t *imu)
{
    if (qmi8658_write_register(
            imu,
            0x60,
            0xB0) != ESP_OK) {
        return ESP_FAIL;
    }

    vTaskDelay(pdMS_TO_TICKS(20));

    if (qmi8658_write_register(
            imu,
            0x02,
            0x60) != ESP_OK ||
        qmi8658_set_accel_range(
            imu,
            QMI8658_ACCEL_RANGE_4G) != ESP_OK ||
        qmi8658_set_accel_odr(
            imu,
            QMI8658_ACCEL_ODR_250HZ) != ESP_OK ||
        qmi8658_set_gyro_range(
            imu,
            QMI8658_GYRO_RANGE_256DPS) != ESP_OK ||
        qmi8658_set_gyro_odr(
            imu,
            QMI8658_GYRO_ODR_250HZ) != ESP_OK) {
        return ESP_FAIL;
    }

    qmi8658_set_accel_unit_mps2(imu, true);
    qmi8658_set_gyro_unit_dps(imu, true);

    return qmi8658_enable_sensors(
        imu,
        QMI8658_ENABLE_ACCEL |
            QMI8658_ENABLE_GYRO);
}

static void imu_task(void *argument)
{
    (void)argument;

    esp_err_t result = bsp_i2c_init();
    if (result != ESP_OK &&
        result != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "I2C initialization failed");
        vTaskDelete(NULL);
        return;
    }

    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    uint8_t address = 0;

    if (bus == NULL ||
        detect_address(bus, &address) != ESP_OK) {
        ESP_LOGE(TAG, "QMI8658 not found");
        vTaskDelete(NULL);
        return;
    }

    qmi8658_dev_t imu = {0};

    if (qmi8658_init(
            &imu,
            bus,
            address) != ESP_OK ||
        setup_imu(&imu) != ESP_OK) {
        ESP_LOGE(TAG, "QMI8658 setup failed");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(
        TAG,
        "Shake detector and motion wake ready");

    int shake_hits = 0;
    int motion_hits = 0;
    int64_t first_shake_hit_us = 0;
    int64_t last_shake_us = 0;
    int64_t last_motion_us = 0;

    while (true) {
        bool ready = false;

        if (qmi8658_is_data_ready(
                &imu,
                &ready) == ESP_OK &&
            ready) {
            qmi8658_data_t data = {0};

            if (qmi8658_read_sensor_data(
                    &imu,
                    &data) == ESP_OK) {
                float magnitude = sqrtf(
                    data.accelX * data.accelX +
                    data.accelY * data.accelY +
                    data.accelZ * data.accelZ);

                float delta = fabsf(
                    magnitude - 9.80665f);

                int64_t now_us =
                    esp_timer_get_time();

                if (delta >= MOTION_WAKE_DELTA_MPS2) {
                    ++motion_hits;

                    if (motion_hits >=
                            MOTION_REQUIRED_SAMPLES &&
                        now_us - last_motion_us >=
                            MOTION_COOLDOWN_US) {
                        motion_hits = 0;
                        last_motion_us = now_us;

                        if (s_motion_callback != NULL) {
                            s_motion_callback(s_context);
                        }
                    }
                } else {
                    motion_hits = 0;
                }

                if (delta >= s_shake_threshold) {
                    if (shake_hits == 0 ||
                        now_us - first_shake_hit_us >
                            SHAKE_HIT_WINDOW_US) {
                        shake_hits = 1;
                        first_shake_hit_us = now_us;
                    } else {
                        ++shake_hits;
                    }

                    if (shake_hits >=
                            SHAKE_REQUIRED_HITS &&
                        now_us - last_shake_us >
                            SHAKE_COOLDOWN_US) {
                        shake_hits = 0;
                        last_shake_us = now_us;
                        ESP_LOGI(TAG, "Shake detected");

                        if (s_shake_callback != NULL) {
                            s_shake_callback(s_context);
                        }
                    }
                } else if (
                    shake_hits != 0 &&
                    now_us - first_shake_hit_us >
                        SHAKE_HIT_WINDOW_US) {
                    shake_hits = 0;
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
    }
}

bool dice_imu_start(
    dice_imu_shake_callback_t shake_callback,
    dice_imu_motion_callback_t motion_callback,
    void *context)
{
    s_shake_callback = shake_callback;
    s_motion_callback = motion_callback;
    s_context = context;

    return xTaskCreate(
               imu_task,
               "dice_imu",
               6144,
               NULL,
               5,
               NULL) == pdPASS;
}

void dice_imu_set_sensitivity(int level)
{
    if (level <= 0) {
        s_shake_threshold = 8.0f;
    } else if (level >= 2) {
        s_shake_threshold = 16.0f;
    } else {
        s_shake_threshold = 12.0f;
    }
}
