#include "dice_transfer.h"

#include <stdint.h>

#include "bsp/esp-bsp.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_private/usb_phy.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tinyusb_msc.h"
#include "wear_levelling.h"

#define TRANSFER_NAMESPACE "dice"
#define TRANSFER_KEY "usb_mode"
#define STORAGE_PARTITION_LABEL "storage"

#define AXP2101_I2C_ADDRESS 0x34
#define AXP2101_STATUS_REG 0x00
#define AXP2101_VBUS_GOOD_MASK (1U << 5)

#define BOOT_GPIO GPIO_NUM_0
#define VBUS_POLL_MS 100
#define VBUS_REMOVAL_DEBOUNCE_MS 1000
#define TRANSFER_START_GRACE_MS 1800
#define BOOT_DEBOUNCE_MS 60
#define BOOT_RELEASE_TIMEOUT_MS 3000
#define USB_SHUTDOWN_DELAY_MS 350

#define TUSB_DESC_TOTAL_LEN \
    (TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN)

static const char *TAG = "dice_transfer";
static tinyusb_msc_storage_handle_t s_storage_handle;
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;
static usb_phy_handle_t s_serial_jtag_phy;

enum {
    ITF_NUM_MSC = 0,
    ITF_NUM_TOTAL,
};

enum {
    EDPT_MSC_OUT = 0x01,
    EDPT_MSC_IN = 0x81,
};

static tusb_desc_device_t s_device_descriptor = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x303A,
    .idProduct = 0x4002,
    .bcdDevice = 0x0100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01,
};

static const uint8_t s_configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(
        1,
        ITF_NUM_TOTAL,
        0,
        TUSB_DESC_TOTAL_LEN,
        TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP,
        100),
    TUD_MSC_DESCRIPTOR(
        ITF_NUM_MSC,
        0,
        EDPT_MSC_OUT,
        EDPT_MSC_IN,
        64),
};

static const char *s_string_descriptors[] = {
    (const char[]){0x09, 0x04},
    "Spiffy Roller",
    "Spiffy Roller Storage",
    "SPIFFY01",
    "Spiffy Storage",
};

bool dice_transfer_initialize_flag_storage(void)
{
    esp_err_t result = nvs_flash_init();

    if (result == ESP_ERR_NVS_NO_FREE_PAGES ||
        result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS requires erase and reinitialization");
        ESP_ERROR_CHECK(nvs_flash_erase());
        result = nvs_flash_init();
    }

    if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "NVS initialization failed: %s",
            esp_err_to_name(result));
        return false;
    }

    return true;
}

static bool write_transfer_flag(uint8_t value)
{
    nvs_handle_t handle;
    esp_err_t result = nvs_open(
        TRANSFER_NAMESPACE,
        NVS_READWRITE,
        &handle);

    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Could not open NVS namespace");
        return false;
    }

    result = nvs_set_u8(handle, TRANSFER_KEY, value);
    if (result == ESP_OK) {
        result = nvs_commit(handle);
    }

    nvs_close(handle);

    if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Could not store transfer flag: %s",
            esp_err_to_name(result));
        return false;
    }

    return true;
}

bool dice_transfer_was_requested(void)
{
    nvs_handle_t handle;
    esp_err_t result = nvs_open(
        TRANSFER_NAMESPACE,
        NVS_READWRITE,
        &handle);

    if (result != ESP_OK) {
        return false;
    }

    uint8_t value = 0;
    result = nvs_get_u8(handle, TRANSFER_KEY, &value);

    if (result == ESP_OK && value != 0) {
        /*
         * The flag is one-shot. Clearing it before entering MSC guarantees
         * that a reset, power loss, or manual BOOT exit returns to dice mode.
         */
        nvs_set_u8(handle, TRANSFER_KEY, 0);
        nvs_commit(handle);
    }

    nvs_close(handle);
    return result == ESP_OK && value != 0;
}

bool dice_transfer_request_and_reboot(void)
{
    if (!write_transfer_flag(1)) {
        return false;
    }

    ESP_LOGI(TAG, "Rebooting into USB mass-storage mode");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return true;
}

static esp_err_t initialize_storage(void)
{
    const esp_partition_t *partition =
        esp_partition_find_first(
            ESP_PARTITION_TYPE_DATA,
            ESP_PARTITION_SUBTYPE_DATA_FAT,
            STORAGE_PARTITION_LABEL);

    if (partition == NULL) {
        ESP_LOGE(
            TAG,
            "FAT partition '%s' not found",
            STORAGE_PARTITION_LABEL);
        return ESP_ERR_NOT_FOUND;
    }

    ESP_RETURN_ON_ERROR(
        wl_mount(partition, &s_wl_handle),
        TAG,
        "Wear levelling mount failed");

    tinyusb_msc_storage_config_t storage_config = {
        .mount_point = TINYUSB_MSC_STORAGE_MOUNT_USB,
        .fat_fs = {
            .base_path = NULL,
            .config.max_files = 5,
            .format_flags = 0,
        },
    };

    storage_config.medium.wl_handle = s_wl_handle;

    ESP_RETURN_ON_ERROR(
        tinyusb_msc_new_storage_spiflash(
            &storage_config,
            &s_storage_handle),
        TAG,
        "MSC storage initialization failed");

    return ESP_OK;
}

static esp_err_t initialize_usb(void)
{
    tinyusb_config_t config = TINYUSB_DEFAULT_CONFIG();

    config.descriptor.device = &s_device_descriptor;
    config.descriptor.full_speed_config =
        s_configuration_descriptor;
    config.descriptor.string = s_string_descriptors;
    config.descriptor.string_count =
        sizeof(s_string_descriptors) /
        sizeof(s_string_descriptors[0]);

    return tinyusb_driver_install(&config);
}

static bool initialize_vbus_reader(
    i2c_master_dev_handle_t *device)
{
    esp_err_t result = bsp_i2c_init();
    if (result != ESP_OK &&
        result != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(
            TAG,
            "I2C initialization failed; unplug reboot unavailable");
        return false;
    }

    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (bus == NULL) {
        ESP_LOGW(TAG, "No BSP I2C handle");
        return false;
    }

    i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AXP2101_I2C_ADDRESS,
        .scl_speed_hz = 400000,
    };

    result = i2c_master_bus_add_device(
        bus,
        &config,
        device);

    if (result != ESP_OK) {
        ESP_LOGW(
            TAG,
            "Could not attach AXP2101 reader: %s",
            esp_err_to_name(result));
        return false;
    }

    return true;
}

static bool read_vbus_good(
    i2c_master_dev_handle_t device,
    bool *vbus_good)
{
    uint8_t register_address = AXP2101_STATUS_REG;
    uint8_t status = 0;

    esp_err_t result = i2c_master_transmit_receive(
        device,
        &register_address,
        1,
        &status,
        1,
        100);

    if (result != ESP_OK) {
        return false;
    }

    *vbus_good =
        (status & AXP2101_VBUS_GOOD_MASK) != 0;
    return true;
}

static void configure_boot_button(void)
{
    gpio_config_t config = {
        .pin_bit_mask = 1ULL << BOOT_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    gpio_config(&config);
}

static void exit_transfer_mode(bool wait_for_boot_release)
{
    ESP_LOGI(TAG, "Stopping USB mass-storage mode");

    esp_err_t result = tinyusb_driver_uninstall();
    if (result != ESP_OK) {
        ESP_LOGW(
            TAG,
            "TinyUSB uninstall returned: %s",
            esp_err_to_name(result));
    }

    /*
     * On ESP32-S3, TinyUSB and USB-Serial-JTAG share the internal PHY.
     * A software restart alone may leave that PHY assigned to USB-OTG.
     * Explicitly recreate the Serial/JTAG PHY before restarting.
     */
    usb_phy_config_t phy_config = {
        .controller = USB_PHY_CTRL_SERIAL_JTAG,
        .target = USB_PHY_TARGET_INT,
    };

    result = usb_new_phy(
        &phy_config,
        &s_serial_jtag_phy);

    if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "USB-Serial-JTAG PHY restore failed: %s",
            esp_err_to_name(result));
    } else {
        ESP_LOGI(TAG, "USB-Serial-JTAG PHY restored");
    }

    /*
     * Do not restart while GPIO0 is still held low. Besides being the BOOT
     * strap, a held button can interfere with a clean transition.
     */
    if (wait_for_boot_release) {
        uint32_t waited_ms = 0;

        while (gpio_get_level(BOOT_GPIO) == 0 &&
               waited_ms < BOOT_RELEASE_TIMEOUT_MS) {
            vTaskDelay(pdMS_TO_TICKS(20));
            waited_ms += 20;
        }
    }

    /*
     * Give Windows time to observe the USB disconnect and let the shared
     * ESP32-S3 USB PHY settle before the USB-Serial-JTAG controller reclaims
     * it during normal boot.
     */
    vTaskDelay(pdMS_TO_TICKS(USB_SHUTDOWN_DELAY_MS));
    esp_restart();
}

void dice_transfer_run(void)
{
    ESP_LOGI(TAG, "Starting USB mass-storage mode");

    ESP_ERROR_CHECK(initialize_storage());
    ESP_ERROR_CHECK(initialize_usb());

    ESP_LOGI(
        TAG,
        "USB storage active; dice application is not running");

    configure_boot_button();

    i2c_master_dev_handle_t axp_device = NULL;
    bool vbus_reader_available =
        initialize_vbus_reader(&axp_device);

    bool vbus_seen = false;
    uint32_t vbus_missing_ms = 0;
    bool boot_was_pressed = false;
    uint32_t boot_pressed_ms = 0;

    vTaskDelay(pdMS_TO_TICKS(TRANSFER_START_GRACE_MS));

    while (true) {
        bool boot_pressed = gpio_get_level(BOOT_GPIO) == 0;

        if (boot_pressed) {
            if (!boot_was_pressed) {
                boot_was_pressed = true;
                boot_pressed_ms = 0;
            } else {
                boot_pressed_ms += VBUS_POLL_MS;
            }

            if (boot_pressed_ms >= BOOT_DEBOUNCE_MS) {
                ESP_LOGI(
                    TAG,
                    "BOOT pressed; leaving transfer mode");
                exit_transfer_mode(true);
            }
        } else {
            boot_was_pressed = false;
            boot_pressed_ms = 0;
        }

        if (vbus_reader_available) {
            bool vbus_good = false;

            if (read_vbus_good(
                    axp_device,
                    &vbus_good)) {
                if (vbus_good) {
                    vbus_seen = true;
                    vbus_missing_ms = 0;
                } else if (vbus_seen) {
                    vbus_missing_ms += VBUS_POLL_MS;

                    if (vbus_missing_ms >=
                        VBUS_REMOVAL_DEBOUNCE_MS) {
                        ESP_LOGI(
                            TAG,
                            "USB cable removed; rebooting");
                        exit_transfer_mode(false);
                    }
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(VBUS_POLL_MS));
    }
}
