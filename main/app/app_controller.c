#include "app_controller.h"

#include <stdio.h>
#include <string.h>

#include "bsp/esp-bsp.h"
#include "dice_audio.h"
#include "dice_battery.h"
#include "dice_power.h"
#include "dice_imu.h"
#include "dice_storage.h"
#include "dice_transfer.h"
#include "dice_state.h"
#include "dice_model.h"
#include "dice_custom_runtime.h"
#include "dice_set_catalog.h"
#include "dice_set_archive.h"
#include "dice_rules.h"
#include "dice_ui.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

typedef enum {
    APP_EVENT_SHAKE,
    APP_EVENT_MOTION,
    APP_EVENT_UI,
    APP_EVENT_BOOT_SHORT,
    APP_EVENT_BOOT_LONG,
} app_event_type_t;

typedef struct {
    app_event_type_t type;
    dice_ui_event_t ui;
} app_event_t;

static const char *TAG = "app_controller";
static QueueHandle_t s_queue;
static dice_pool_t s_pool;
static bool s_menu_visible;
static bool s_sound_enabled = true;
static int s_brightness = 65;
static int s_shake_level = 1;
static int s_timeout_seconds = 60;
static int s_selector_hide_ms = 1000;
static bool s_low_battery_warning_shown;
static size_t s_active_set_index;
static char s_active_set_id[DICE_SET_ID_MAX + 1] = "standard";
static dice_custom_pool_t s_custom_pools[DICE_SET_MAX_SETS];
static dice_rule_definition_t *s_rule_definitions;
static dice_rule_state_t *s_rule_states;
static dice_custom_roll_result_t s_last_custom_roll;
static bool s_last_custom_roll_valid;

static bool allocate_rule_storage(void)
{
    if (s_rule_definitions != NULL &&
        s_rule_states != NULL) {
        memset(
            s_rule_definitions,
            0,
            sizeof(*s_rule_definitions) *
                DICE_SET_MAX_SETS);
        memset(
            s_rule_states,
            0,
            sizeof(*s_rule_states) *
                DICE_SET_MAX_SETS);
        return true;
    }

    size_t definition_bytes =
        sizeof(*s_rule_definitions) *
        DICE_SET_MAX_SETS;
    size_t state_bytes =
        sizeof(*s_rule_states) *
        DICE_SET_MAX_SETS;

    s_rule_definitions = heap_caps_calloc(
        DICE_SET_MAX_SETS,
        sizeof(*s_rule_definitions),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    s_rule_states = heap_caps_calloc(
        DICE_SET_MAX_SETS,
        sizeof(*s_rule_states),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (s_rule_definitions == NULL ||
        s_rule_states == NULL) {
        if (s_rule_definitions != NULL) {
            free(s_rule_definitions);
            s_rule_definitions = NULL;
        }

        if (s_rule_states != NULL) {
            free(s_rule_states);
            s_rule_states = NULL;
        }

        ESP_LOGE(
            TAG,
            "Could not allocate rule tables in PSRAM");
        return false;
    }

    ESP_LOGI(
        TAG,
        "Allocated %u bytes of rule tables in PSRAM",
        (unsigned)(definition_bytes + state_bytes));

    return true;
}

static bool post_event(const app_event_t *event)
{
    return s_queue != NULL &&
           xQueueSend(s_queue, event, 0) == pdTRUE;
}

static void ui_callback(
    const dice_ui_event_t *event,
    void *context)
{
    (void)context;
    if (event == NULL) {
        return;
    }

    app_event_t app_event = {
        .type = APP_EVENT_UI,
        .ui = *event,
    };
    post_event(&app_event);
}

void app_controller_notify_shake(void *context)
{
    (void)context;
    const app_event_t event = {.type = APP_EVENT_SHAKE};
    post_event(&event);
}

static void app_controller_notify_motion(void *context)
{
    (void)context;
    const app_event_t event = {.type = APP_EVENT_MOTION};
    post_event(&event);
}

void app_controller_notify_boot(bool long_press)
{
    const app_event_t event = {
        .type = long_press
            ? APP_EVENT_BOOT_LONG
            : APP_EVENT_BOOT_SHORT,
    };
    post_event(&event);
}

static void battery_update_callback(
    bool available,
    uint16_t voltage_mv,
    uint8_t percent,
    bool usb_power_present,
    void *context)
{
    (void)context;

    dice_ui_set_battery_status(
        available,
        voltage_mv,
        percent);

    dice_power_update_battery(
        available,
        voltage_mv,
        usb_power_present);

    if (available &&
        !usb_power_present &&
        voltage_mv <= 3550 &&
        !s_low_battery_warning_shown) {
        s_low_battery_warning_shown = true;
        dice_ui_show_notice(
            "Low battery - please charge");
    }

    if (!available ||
        usb_power_present ||
        voltage_mv >= 3650) {
        s_low_battery_warning_shown = false;
    }
}

static void save_current_state(void)
{
    dice_saved_state_t state = {
        .pool = s_pool,
        .sound_enabled = s_sound_enabled,
        .brightness = s_brightness,
        .shake_level = s_shake_level,
        .display_timeout_seconds = s_timeout_seconds,
        .selector_hide_ms = s_selector_hide_ms,
    };
    snprintf(
        state.active_set_id,
        sizeof(state.active_set_id),
        "%s",
        s_active_set_id);

    dice_state_save(&state);
}

static void shutdown_callback(
    const char *reason,
    void *context)
{
    (void)context;

    ESP_LOGW(TAG, "Preparing for power-off: %s", reason);
    save_current_state();
    dice_audio_stop();
    dice_storage_unmount();

    dice_ui_show_notice(
        strcmp(reason, "Low battery protection") == 0
            ? "Battery low - shutting down"
            : "Sleeping - press PWR to wake");

    vTaskDelay(pdMS_TO_TICKS(900));
    bsp_display_brightness_set(0);
}

static void update_menu_values(void)
{
    dice_ui_set_menu_values(
        s_sound_enabled,
        s_brightness,
        s_shake_level,
        s_timeout_seconds,
        s_selector_hide_ms);
}

static void update_rule_options_ui(void)
{
    if (s_active_set_index == 0 ||
        !s_rule_definitions[s_active_set_index].loaded) {
        dice_ui_set_rule_options(NULL, 0);
        dice_ui_set_main_rule_option(
            "",
            "",
            false,
            0);
        return;
    }

    const dice_rule_definition_t *definition =
        &s_rule_definitions[s_active_set_index];
    const dice_rule_state_t *state =
        &s_rule_states[s_active_set_index];
    dice_ui_rule_option_t options[DICE_UI_MAX_RULE_OPTIONS] = {0};

    bool main_option_found = false;

    for (size_t i = 0; i < definition->control_count; ++i) {
        snprintf(
            options[i].label,
            sizeof(options[i].label),
            "%s",
            definition->controls[i].label);

        options[i].toggle =
            definition->controls[i].type ==
                DICE_RULE_CONTROL_TOGGLE;

        dice_rules_format_control_value(
            definition,
            state,
            i,
            options[i].value,
            sizeof(options[i].value));

        if (!main_option_found &&
            definition->controls[i].display_main) {
            dice_ui_set_main_rule_option(
                options[i].label,
                options[i].value,
                true,
                i);
            main_option_found = true;
        }
    }

    if (!main_option_found) {
        dice_ui_set_main_rule_option(
            "",
            "",
            false,
            0);
    }

    dice_ui_set_rule_options(
        options,
        definition->control_count);
}

static void prepare_custom_result_ui(
    const dice_set_definition_t *set,
    const dice_custom_roll_result_t *result)
{
    char title[64] = {0};
    char details[96] = {0};
    const dice_rule_definition_t *definition =
        &s_rule_definitions[s_active_set_index];
    dice_rule_state_t *state =
        &s_rule_states[s_active_set_index];

    if (definition->loaded && definition->result_count > 0) {
        dice_rules_evaluate(
            definition,
            state,
            result,
            title,
            sizeof(title),
            details,
            sizeof(details));
    } else {
        snprintf(title, sizeof(title), "Total: %ld", (long)result->numeric_total);
    }

    dice_ui_set_custom_result_text(title, details);

    bool action_visible = false;
    const char *action_label = NULL;
    size_t action_index = 0;
    if (definition->loaded) {
        for (size_t i = 0; i < definition->action_count; ++i) {
            if (dice_rules_action_available(definition, state, i, result)) {
                action_visible = true;
                action_label = definition->actions[i].label;
                action_index = i;
                break;
            }
        }
    }
    dice_ui_set_custom_action(
        action_label != NULL ? action_label : "",
        action_visible,
        action_index);
    (void)set;
}

static void set_menu_visible(bool visible)
{
    s_menu_visible = visible;
    dice_ui_show_menu(visible);
    if (visible) {
        update_menu_values();
    }
}

static void clear_pool(void)
{
    if (s_active_set_index == 0) {
        memset(&s_pool, 0, sizeof(s_pool));
        dice_ui_set_pool(&s_pool);
    } else {
        dice_custom_pool_clear(
            &s_custom_pools[s_active_set_index]);
        dice_ui_set_custom_pool(
            &s_custom_pools[s_active_set_index]);
    }

    s_last_custom_roll_valid = false;
    dice_ui_set_custom_action("", false, 0);
    set_menu_visible(false);
    dice_ui_show_pool_cleared();
    ESP_LOGI(TAG, "Dice pool cleared");
}

static void perform_roll(void)
{
    dice_power_note_activity();

    if (dice_ui_is_rolling() ||
        s_menu_visible ||
        dice_ui_is_overlay_visible()) {
        return;
    }

    if (s_active_set_index != 0) {
        const dice_set_definition_t *set =
            dice_set_catalog_get(s_active_set_index);

        if (set == NULL) {
            dice_ui_show_notice("Active set is unavailable");
            return;
        }

        dice_custom_pool_t *pool =
            &s_custom_pools[s_active_set_index];

        if (dice_custom_pool_count(
                pool,
                set->die_count) == 0) {
            dice_ui_show_notice("Dice pool is empty");
            return;
        }

        dice_custom_roll_result_t result;

        if (!dice_custom_roll(
                set,
                pool,
                &result)) {
            dice_ui_show_notice("Custom roll failed");
            return;
        }

        ESP_LOGI(
            TAG,
            "Rolling %u custom dice, numeric total=%ld",
            (unsigned)result.total_count,
            (long)result.numeric_total);

        s_last_custom_roll = result;
        s_last_custom_roll_valid = true;
        memset(
            s_rule_states[s_active_set_index].action_used,
            0,
            sizeof(s_rule_states[s_active_set_index].action_used));
        prepare_custom_result_ui(set, &s_last_custom_roll);
        dice_audio_play_roll();
        dice_ui_start_custom_roll_animation(
            set,
            &s_last_custom_roll);
        return;
    }

    if (dice_model_pool_count(&s_pool) == 0) {
        dice_ui_show_notice("Dice pool is empty");
        return;
    }

    dice_roll_result_t result;
    if (!dice_model_roll(&s_pool, &result)) {
        return;
    }

    ESP_LOGI(
        TAG,
        "Rolling %u dice, total=%ld",
        (unsigned)result.total_count,
        (long)result.numeric_total);
    dice_audio_play_roll();
    dice_ui_start_roll_animation(&result);
}

static void handle_ui_event(const dice_ui_event_t *event)
{
    dice_power_note_activity();

    switch (event->type) {
        case DICE_UI_EVENT_SELECTOR_CHANGED:
            if (!dice_ui_is_rolling() &&
                !s_menu_visible &&
                !dice_ui_is_overlay_visible()) {
                if (s_active_set_index == 0) {
                    dice_model_adjust_quantity(
                        &s_pool,
                        event->selected_type,
                        event->quantity_delta);
                    dice_ui_set_pool(&s_pool);
                } else {
                    dice_custom_pool_adjust(
                        &s_custom_pools[s_active_set_index],
                        event->custom_die_index,
                        event->quantity_delta);
                    dice_ui_set_custom_pool(
                        &s_custom_pools[s_active_set_index]);
                }
            }
            break;

        case DICE_UI_EVENT_CLEAR_POOL:
            clear_pool();
            break;

        case DICE_UI_EVENT_MENU_SOUND:
            s_sound_enabled = !s_sound_enabled;
            dice_audio_set_enabled(s_sound_enabled);
            update_menu_values();
            break;

        case DICE_UI_EVENT_MENU_BRIGHTNESS:
            if (s_brightness == 35) {
                s_brightness = 65;
            } else if (s_brightness == 65) {
                s_brightness = 100;
            } else {
                s_brightness = 35;
            }
            bsp_display_brightness_set(s_brightness);
            update_menu_values();
            break;

        case DICE_UI_EVENT_MENU_SHAKE:
            s_shake_level = (s_shake_level + 1) % 3;
            dice_imu_set_sensitivity(s_shake_level);
            update_menu_values();
            break;

        case DICE_UI_EVENT_MENU_TIMEOUT:
            s_timeout_seconds += event->quantity_delta * 15;
            if (s_timeout_seconds < 0) {
                s_timeout_seconds = 0;
            }
            if (s_timeout_seconds > 300) {
                s_timeout_seconds = 300;
            }
            update_menu_values();
            break;

        case DICE_UI_EVENT_MENU_SELECTOR_DELAY:
            s_selector_hide_ms += event->quantity_delta * 500;
            if (s_selector_hide_ms < 500) {
                s_selector_hide_ms = 500;
            }
            if (s_selector_hide_ms > 5000) {
                s_selector_hide_ms = 5000;
            }
            update_menu_values();
            break;

        case DICE_UI_EVENT_MENU_SET:
            dice_ui_show_set_menu(true);
            break;

        case DICE_UI_EVENT_SET_SELECTED: {
            const dice_set_definition_t *set =
                dice_set_catalog_get(event->set_index);

            if (set == NULL) {
                dice_ui_show_notice("Set is no longer available");
                break;
            }

            if (event->set_index == s_active_set_index) {
                dice_ui_show_set_menu(false);
                break;
            }

            s_active_set_index = event->set_index;
            snprintf(
                s_active_set_id,
                sizeof(s_active_set_id),
                "%s",
                set->id);

            save_current_state();
            dice_ui_show_set_menu(false);
            dice_ui_show_notice("Switching dice set...");

            /*
             * Archive extraction and face-cache replacement are deliberately
             * performed during startup, before the live playfield owns image
             * resources. Rebooting here avoids mutating FATFS and render-cache
             * state while LVGL is actively using the current set.
             */
            vTaskDelay(pdMS_TO_TICKS(250));
            esp_restart();
            break;
        }

        case DICE_UI_EVENT_MENU_SET_OPTIONS:
            if (s_active_set_index == 0 ||
                !s_rule_definitions[s_active_set_index].loaded ||
                s_rule_definitions[s_active_set_index].control_count == 0) {
                dice_ui_show_notice("This set has no options");
            } else {
                update_rule_options_ui();
                dice_ui_show_rule_options(true);
            }
            break;

        case DICE_UI_EVENT_OPTION_CHANGED:
            if (s_active_set_index != 0 &&
                s_rule_definitions[s_active_set_index].loaded) {
                dice_rules_adjust_control(
                    &s_rule_definitions[s_active_set_index],
                    &s_rule_states[s_active_set_index],
                    event->option_index,
                    event->quantity_delta == 0
                        ? 1
                        : event->quantity_delta);
                update_rule_options_ui();
            }
            break;

        case DICE_UI_EVENT_CUSTOM_ACTION: {
            if (s_active_set_index == 0 || !s_last_custom_roll_valid) {
                break;
            }
            const dice_set_definition_t *set =
                dice_set_catalog_get(s_active_set_index);
            bool rerolled_mask[DICE_CUSTOM_MAX_TOTAL] = {0};

            if (set != NULL && dice_rules_apply_action(
                    &s_rule_definitions[s_active_set_index],
                    &s_rule_states[s_active_set_index],
                    event->action_index,
                    &s_last_custom_roll,
                    rerolled_mask,
                    DICE_CUSTOM_MAX_TOTAL)) {
                prepare_custom_result_ui(
                    set,
                    &s_last_custom_roll);
                dice_audio_play_roll();
                dice_ui_start_custom_reroll_animation(
                    set,
                    &s_last_custom_roll,
                    rerolled_mask,
                    DICE_CUSTOM_MAX_TOTAL);
            }
            break;
        }

        case DICE_UI_EVENT_MENU_TRANSFER:
            save_current_state();
            dice_ui_show_notice(
                "Restarting in USB transfer mode");
            vTaskDelay(pdMS_TO_TICKS(700));
            bsp_display_brightness_set(0);
            vTaskDelay(pdMS_TO_TICKS(150));
            dice_transfer_request_and_reboot();
            break;

        case DICE_UI_EVENT_MENU_ABOUT: {
            size_t storage_total = 0;
            size_t storage_used = 0;
            bool storage_available = dice_storage_get_usage(
                &storage_total,
                &storage_used);

            size_t ram_total = heap_caps_get_total_size(
                MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            size_t ram_free = heap_caps_get_free_size(
                MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            size_t ram_used =
                ram_free <= ram_total ? ram_total - ram_free : 0;

            size_t psram_total = heap_caps_get_total_size(
                MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            size_t psram_free = heap_caps_get_free_size(
                MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            size_t psram_used =
                psram_free <= psram_total
                    ? psram_total - psram_free
                    : 0;

            char status[1200];
            const char *footer =
                "\nSpiffy Roller GitHub repo:\n"
                "https://github.com/candre23/SpiffyRoller\n\n"
                "Spiffy Roller Dice Set Maker:\n"
                "https://github.com/candre23/SpiffyRoller_SetMaker\n\n"
                "Spiffy Roller dice sets:\n"
                "https://github.com/candre23/SpiffyRoller_DiceSets\n\n"
                "Spiffy Roller is public domain software.  "
                "Copyleft 2026.  Do what thou wilt shall be "
                "the whole of the law.  One step closer to AGI.";

            if (storage_available) {
                snprintf(
                    status,
                    sizeof(status),
                    "Spiffy Roller v1.0\n\n"
                    "Flash storage partition\n"
                    "  Total: %u KB\n"
                    "  Used:  %u KB\n\n"
                    "RAM\n"
                    "  Total: %u KB\n"
                    "  Used:  %u KB\n\n"
                    "PSRAM\n"
                    "  Total: %u KB\n"
                    "  Used:  %u KB\n%s",
                    (unsigned)(storage_total / 1024),
                    (unsigned)(storage_used / 1024),
                    (unsigned)(ram_total / 1024),
                    (unsigned)(ram_used / 1024),
                    (unsigned)(psram_total / 1024),
                    (unsigned)(psram_used / 1024),
                    footer);
            } else {
                snprintf(
                    status,
                    sizeof(status),
                    "Spiffy Roller v1.0\n\n"
                    "Flash storage partition\n"
                    "  Usage unavailable\n\n"
                    "RAM\n"
                    "  Total: %u KB\n"
                    "  Used:  %u KB\n\n"
                    "PSRAM\n"
                    "  Total: %u KB\n"
                    "  Used:  %u KB\n%s",
                    (unsigned)(ram_total / 1024),
                    (unsigned)(ram_used / 1024),
                    (unsigned)(psram_total / 1024),
                    (unsigned)(psram_used / 1024),
                    footer);
            }

            dice_ui_show_about_status(status);
            break;
        }

        case DICE_UI_EVENT_MENU_CLOSE:
            set_menu_visible(false);
            break;
    }
}

static void controller_task(void *argument)
{
    (void)argument;
    app_event_t event;

    while (true) {
        if (xQueueReceive(
                s_queue,
                &event,
                portMAX_DELAY) != pdTRUE) {
            continue;
        }

        switch (event.type) {
            case APP_EVENT_SHAKE:
                perform_roll();
                break;
            case APP_EVENT_MOTION:
                dice_power_note_activity();
                if (dice_ui_is_display_dimmed()) {
                    dice_ui_wake_display();
                }
                break;
            case APP_EVENT_UI:
                handle_ui_event(&event.ui);
                break;
            case APP_EVENT_BOOT_SHORT:
                dice_power_note_activity();
                set_menu_visible(!s_menu_visible);
                break;
            case APP_EVENT_BOOT_LONG:
                save_current_state();
                dice_ui_show_notice(
                    "Restarting in USB transfer mode");
                vTaskDelay(pdMS_TO_TICKS(700));
                bsp_display_brightness_set(0);
                vTaskDelay(pdMS_TO_TICKS(150));
                dice_transfer_request_and_reboot();
                break;
        }
    }
}

bool app_controller_start(void)
{
    dice_model_init_default_pool(&s_pool);

    dice_saved_state_t saved_state = {0};
    if (dice_state_load(&saved_state)) {
        s_pool = saved_state.pool;
        s_sound_enabled = saved_state.sound_enabled;
        s_brightness = saved_state.brightness;
        s_shake_level = saved_state.shake_level;
        s_timeout_seconds =
            saved_state.display_timeout_seconds;
        s_selector_hide_ms = saved_state.selector_hide_ms;
        snprintf(
            s_active_set_id,
            sizeof(s_active_set_id),
            "%s",
            saved_state.active_set_id[0] != '\0'
                ? saved_state.active_set_id
                : "standard");

        if (s_brightness != 35 &&
            s_brightness != 65 &&
            s_brightness != 100) {
            s_brightness = 65;
        }
        if (s_shake_level < 0 || s_shake_level > 2) {
            s_shake_level = 1;
        }
        if (s_timeout_seconds < 0 ||
            s_timeout_seconds > 300 ||
            (s_timeout_seconds % 15) != 0) {
            s_timeout_seconds = 60;
        }
        if (s_selector_hide_ms < 500 ||
            s_selector_hide_ms > 5000 ||
            (s_selector_hide_ms % 500) != 0) {
            s_selector_hide_ms = 1000;
        }
    }

    dice_ui_startup_status("Initializing controls...");

    s_queue = xQueueCreate(16, sizeof(app_event_t));
    if (s_queue == NULL) {
        return false;
    }

    if (!dice_audio_start()) {
        ESP_LOGW(TAG, "Audio task failed");
    } else {
        dice_audio_set_enabled(s_sound_enabled);
        dice_audio_play_tick();
    }

    if (!dice_power_start(shutdown_callback, NULL)) {
        ESP_LOGW(TAG, "Power management initialization failed");
    }

    dice_ui_startup_status("Mounting storage...");
    if (!dice_storage_mount()) {
        ESP_LOGW(
            TAG,
            "Persistent storage unavailable; "
            "only the Standard set will be available");
    }

    dice_ui_startup_status("Reading dice set catalog...");
    dice_set_catalog_scan();

    if (!allocate_rule_storage()) {
        return false;
    }

    size_t catalog_count = dice_set_catalog_count();
    for (size_t index = 1; index < catalog_count; ++index) {
        const dice_set_definition_t *rule_set =
            dice_set_catalog_get(index);
        if (rule_set != NULL &&
            dice_rules_load(rule_set, &s_rule_definitions[index])) {
            dice_rules_state_defaults(
                &s_rule_definitions[index],
                &s_rule_states[index]);
        }
    }
    s_active_set_index =
        dice_set_catalog_index_of(s_active_set_id);

    const dice_set_definition_t *active_set =
        dice_set_catalog_get(s_active_set_index);
    snprintf(
        s_active_set_id,
        sizeof(s_active_set_id),
        "%s",
        active_set != NULL
            ? active_set->id
            : "standard");

    if (s_active_set_index != 0 && active_set != NULL) {
        dice_ui_startup_status("Preparing active dice set...");
        if (!dice_set_archive_prepare_set(active_set->folder_path)) {
            ESP_LOGW(TAG, "Active set preparation failed");
        }
        dice_rules_load(
            active_set,
            &s_rule_definitions[s_active_set_index]);
        dice_rules_state_defaults(
            &s_rule_definitions[s_active_set_index],
            &s_rule_states[s_active_set_index]);
        dice_ui_startup_status("Rendering active dice faces...");
        dice_ui_startup_preload_set(active_set);
    }

    dice_ui_startup_status("Building playfield...");
    if (!dice_ui_start(ui_callback, NULL, &s_pool)) {
        return false;
    }

    const char *set_names[DICE_SET_MAX_SETS];
    size_t set_count = dice_set_catalog_count();
    for (size_t index = 0;
         index < set_count;
         ++index) {
        set_names[index] =
            dice_set_catalog_get(index)->name;
    }
    dice_ui_set_available_sets(
        set_names,
        set_count,
        s_active_set_index);

    if (s_active_set_index == 0) {
        dice_ui_activate_standard_set(&s_pool);
    } else {
        const dice_set_definition_t *startup_set =
            dice_set_catalog_get(s_active_set_index);

        if (startup_set != NULL) {
            dice_ui_activate_custom_set(
                startup_set,
                &s_custom_pools[s_active_set_index]);
        }
    }

    update_rule_options_ui();

    bsp_display_brightness_set(s_brightness);

    if (!dice_imu_start(
            app_controller_notify_shake,
            app_controller_notify_motion,
            NULL)) {
        ESP_LOGW(TAG, "IMU task failed");
    }
    if (!dice_battery_start(
            battery_update_callback,
            NULL)) {
        ESP_LOGW(TAG, "Battery task failed");
    }

    dice_audio_set_enabled(s_sound_enabled);
    dice_imu_set_sensitivity(s_shake_level);
    update_menu_values();

    return xTaskCreate(
               controller_task,
               "app_controller",
               6144,
               NULL,
               6,
               NULL) == pdPASS;
}
