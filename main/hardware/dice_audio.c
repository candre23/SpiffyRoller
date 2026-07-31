#include "dice_audio.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bsp/esp-bsp.h"
#include "dice_storage.h"
#include "driver/i2s_std.h"
#include "esp_check.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define AUDIO_RATE 16000
#define AUDIO_VOLUME 78
#define TWO_PI_F 6.28318530717958647692f
#define WAV_STREAM_CHUNK 1024
#define FALLBACK_TICK_MIN 28
#define FALLBACK_TICK_VARIATION 18

typedef enum {
    AUDIO_COMMAND_ROLL,
    AUDIO_COMMAND_TICK,
} audio_command_t;

typedef struct {
    size_t data_offset;
    size_t data_size;
} wav_info_t;

static const char *TAG = "dice_audio";
static i2s_chan_handle_t s_tx;
static i2s_chan_handle_t s_rx;
static esp_codec_dev_handle_t s_speaker;
static TaskHandle_t s_task;
static QueueHandle_t s_queue;
static volatile bool s_stop_requested;
static bool s_enabled = true;

static uint32_t read_u32_le(const uint8_t *data)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static uint16_t read_u16_le(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static esp_err_t initialize_audio(void)
{
    i2s_chan_config_t channel_config =
        I2S_CHANNEL_DEFAULT_CONFIG(CONFIG_BSP_I2S_NUM, I2S_ROLE_MASTER);
    channel_config.auto_clear = true;

    ESP_RETURN_ON_ERROR(
        i2s_new_channel(&channel_config, &s_tx, &s_rx),
        TAG, "I2S channel");

    const i2s_std_config_t i2s_config = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_RATE),
        .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT,
            I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = BSP_I2S_MCLK,
            .bclk = BSP_I2S_SCLK,
            .ws = BSP_I2S_LCLK,
            .dout = BSP_I2S_DOUT,
            .din = BSP_I2S_DSIN,
            .invert_flags = {false, false, false},
        },
    };

    ESP_RETURN_ON_ERROR(
        i2s_channel_init_std_mode(s_tx, &i2s_config),
        TAG, "I2S TX");
    ESP_RETURN_ON_ERROR(
        i2s_channel_init_std_mode(s_rx, &i2s_config),
        TAG, "I2S RX");

    audio_codec_i2s_cfg_t data_config = {
        .port = CONFIG_BSP_I2S_NUM,
        .tx_handle = s_tx,
        .rx_handle = s_rx,
    };
    const audio_codec_data_if_t *data_if =
        audio_codec_new_i2s_data(&data_config);
    ESP_RETURN_ON_FALSE(data_if, ESP_FAIL, TAG, "data interface");

    esp_err_t i2c_result = bsp_i2c_init();
    if (i2c_result != ESP_OK && i2c_result != ESP_ERR_INVALID_STATE) {
        return i2c_result;
    }

    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();
    audio_codec_i2c_cfg_t control_config = {
        .port = BSP_I2C_NUM,
        .addr = ES8311_CODEC_DEFAULT_ADDR,
        .bus_handle = bsp_i2c_get_handle(),
    };
    const audio_codec_ctrl_if_t *control_if =
        audio_codec_new_i2c_ctrl(&control_config);

    esp_codec_dev_hw_gain_t gain = {
        .pa_voltage = 5.0,
        .codec_dac_voltage = 3.3,
    };
    es8311_codec_cfg_t codec_config = {
        .ctrl_if = control_if,
        .gpio_if = gpio_if,
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC,
        .pa_pin = BSP_POWER_AMP_IO,
        .pa_reverted = false,
        .master_mode = false,
        .use_mclk = true,
        .digital_mic = false,
        .invert_mclk = false,
        .invert_sclk = false,
        .hw_gain = gain,
    };

    const audio_codec_if_t *codec_if = es8311_codec_new(&codec_config);
    ESP_RETURN_ON_FALSE(codec_if, ESP_FAIL, TAG, "codec");

    esp_codec_dev_cfg_t device_config = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = codec_if,
        .data_if = data_if,
    };
    s_speaker = esp_codec_dev_new(&device_config);
    ESP_RETURN_ON_FALSE(s_speaker, ESP_FAIL, TAG, "speaker");

    esp_codec_dev_sample_info_t sample_info = {
        .bits_per_sample = 16,
        .channel = 1,
        .channel_mask = 0,
        .sample_rate = AUDIO_RATE,
        .mclk_multiple = 256,
    };

    ESP_RETURN_ON_FALSE(
        esp_codec_dev_open(s_speaker, &sample_info) ==
            ESP_CODEC_DEV_OK,
        ESP_FAIL, TAG, "speaker open");
    ESP_RETURN_ON_FALSE(
        esp_codec_dev_set_out_vol(s_speaker, AUDIO_VOLUME) ==
            ESP_CODEC_DEV_OK,
        ESP_FAIL, TAG, "volume");

    return ESP_OK;
}

static bool read_exact(FILE *file, void *buffer, size_t size)
{
    return fread(buffer, 1, size, file) == size;
}

static bool inspect_wav(FILE *file, wav_info_t *info)
{
    uint8_t header[12];
    if (!read_exact(file, header, sizeof(header)) ||
        memcmp(header, "RIFF", 4) != 0 ||
        memcmp(header + 8, "WAVE", 4) != 0) {
        return false;
    }

    bool format_ok = false;

    while (true) {
        uint8_t chunk_header[8];
        if (!read_exact(file, chunk_header, sizeof(chunk_header))) {
            return false;
        }

        uint32_t chunk_size = read_u32_le(chunk_header + 4);
        long chunk_data_offset = ftell(file);
        if (chunk_data_offset < 0) {
            return false;
        }

        if (memcmp(chunk_header, "fmt ", 4) == 0) {
            if (chunk_size < 16) {
                return false;
            }

            uint8_t format_data[16];
            if (!read_exact(file, format_data, sizeof(format_data))) {
                return false;
            }

            uint16_t format = read_u16_le(format_data);
            uint16_t channels = read_u16_le(format_data + 2);
            uint32_t sample_rate = read_u32_le(format_data + 4);
            uint16_t bits = read_u16_le(format_data + 14);

            format_ok =
                format == 1 &&
                channels == 1 &&
                sample_rate == AUDIO_RATE &&
                bits == 16;
        } else if (memcmp(chunk_header, "data", 4) == 0) {
            if (!format_ok || chunk_size == 0) {
                return false;
            }

            info->data_offset = (size_t)chunk_data_offset;
            info->data_size = chunk_size;
            return true;
        }

        long next_chunk =
            chunk_data_offset +
            (long)chunk_size +
            (long)(chunk_size & 1U);

        if (fseek(file, next_chunk, SEEK_SET) != 0) {
            return false;
        }
    }
}

static void synthesize_tick(
    float frequency_hz,
    float amplitude,
    size_t sample_count)
{
    if (sample_count > 240) {
        sample_count = 240;
    }

    int16_t frame[240];
    float phase = 0.0f;
    float step = TWO_PI_F * frequency_hz / AUDIO_RATE;

    for (size_t index = 0; index < sample_count; ++index) {
        float envelope =
            1.0f - (float)index / (float)sample_count;
        frame[index] = (int16_t)(
            sinf(phase) *
            amplitude *
            envelope *
            envelope);
        phase += step;
    }

    esp_codec_dev_write(
        s_speaker,
        frame,
        sample_count * sizeof(frame[0]));
}

static void play_tick_sound(void)
{
    synthesize_tick(1500.0f, 7000.0f, 160);
}

static void play_fallback_roll(void)
{
    const uint32_t tick_count =
        FALLBACK_TICK_MIN +
        esp_random() % FALLBACK_TICK_VARIATION;

    s_stop_requested = false;

    for (uint32_t index = 0;
         index < tick_count && !s_stop_requested;
         ++index) {
        float progress =
            (float)index / (float)(tick_count - 1U);

        float frequency =
            1150.0f +
            (float)(esp_random() % 1100U);

        float amplitude =
            4200.0f +
            (float)(esp_random() % 2800U);

        size_t samples =
            75U + (esp_random() % 75U);

        synthesize_tick(
            frequency,
            amplitude * (1.0f - 0.30f * progress),
            samples);

        uint32_t base_delay =
            12U + (uint32_t)(progress * progress * 70.0f);
        uint32_t random_delay =
            esp_random() % 19U;

        vTaskDelay(pdMS_TO_TICKS(base_delay + random_delay));
    }
}

static bool play_roll_file(void)
{
    if (!dice_storage_is_mounted()) {
        ESP_LOGW(
            TAG,
            "Storage unavailable; using generated roll sound");
        return false;
    }

    FILE *file = fopen(DICE_ROLL_WAV_PATH, "rb");
    if (file == NULL) {
        ESP_LOGI(
            TAG,
            "%s not found; using generated roll sound",
            DICE_ROLL_WAV_PATH);
        return false;
    }

    wav_info_t info = {0};
    if (!inspect_wav(file, &info)) {
        ESP_LOGW(
            TAG,
            "%s is not mono 16-bit PCM at 16000 Hz; "
            "using generated roll sound",
            DICE_ROLL_WAV_PATH);
        fclose(file);
        return false;
    }

    if (fseek(file, (long)info.data_offset, SEEK_SET) != 0) {
        fclose(file);
        return false;
    }

    ESP_LOGI(TAG, "Playing custom roll sound");

    uint8_t buffer[WAV_STREAM_CHUNK];
    size_t remaining = info.data_size;
    s_stop_requested = false;

    while (remaining > 0 && !s_stop_requested) {
        size_t requested =
            remaining < sizeof(buffer)
                ? remaining
                : sizeof(buffer);

        size_t received =
            fread(buffer, 1, requested, file);

        if (received == 0) {
            break;
        }

        if (esp_codec_dev_write(
                s_speaker,
                buffer,
                received) != ESP_CODEC_DEV_OK) {
            break;
        }

        remaining -= received;
    }

    fclose(file);
    return remaining == 0;
}

static void play_roll_sound(void)
{
    if (!play_roll_file()) {
        play_fallback_roll();
    }
}

static void audio_task(void *argument)
{
    (void)argument;

    if (initialize_audio() != ESP_OK) {
        ESP_LOGE(TAG, "Initialization failed");
        vTaskDelete(NULL);
        return;
    }

    audio_command_t command;

    while (true) {
        if (xQueueReceive(
                s_queue,
                &command,
                portMAX_DELAY) != pdTRUE) {
            continue;
        }

        if (!s_enabled) {
            continue;
        }

        if (command == AUDIO_COMMAND_TICK) {
            play_tick_sound();
        } else {
            play_roll_sound();
        }
    }
}

static bool queue_command(audio_command_t command)
{
    return s_queue != NULL &&
           s_enabled &&
           xQueueSend(s_queue, &command, 0) == pdTRUE;
}

bool dice_audio_start(void)
{
    if (s_task != NULL) {
        return true;
    }

    s_queue = xQueueCreate(8, sizeof(audio_command_t));
    if (s_queue == NULL) {
        return false;
    }

    return xTaskCreate(
               audio_task,
               "dice_audio",
               7168,
               NULL,
               5,
               &s_task) == pdPASS;
}

bool dice_audio_play_roll(void)
{
    return queue_command(AUDIO_COMMAND_ROLL);
}

bool dice_audio_play_tick(void)
{
    return queue_command(AUDIO_COMMAND_TICK);
}

void dice_audio_set_enabled(bool enabled)
{
    s_enabled = enabled;
}

bool dice_audio_is_enabled(void)
{
    return s_enabled;
}

void dice_audio_stop(void)
{
    s_stop_requested = true;
}
