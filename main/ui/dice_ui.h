#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "dice_model.h"
#include "dice_custom_runtime.h"
#include "dice_set_catalog.h"

typedef enum {
    DICE_UI_EVENT_SELECTOR_CHANGED,
    DICE_UI_EVENT_CLEAR_POOL,
    DICE_UI_EVENT_MENU_SOUND,
    DICE_UI_EVENT_MENU_BRIGHTNESS,
    DICE_UI_EVENT_MENU_SHAKE,
    DICE_UI_EVENT_MENU_TIMEOUT,
    DICE_UI_EVENT_MENU_SELECTOR_DELAY,
    DICE_UI_EVENT_MENU_TRANSFER,
    DICE_UI_EVENT_MENU_SET,
    DICE_UI_EVENT_MENU_SET_OPTIONS,
    DICE_UI_EVENT_OPTION_CHANGED,
    DICE_UI_EVENT_CUSTOM_ACTION,
    DICE_UI_EVENT_SET_SELECTED,
    DICE_UI_EVENT_MENU_ABOUT,
    DICE_UI_EVENT_MENU_CLOSE,
} dice_ui_event_type_t;

typedef struct {
    dice_ui_event_type_t type;
    dice_type_id_t selected_type;
    int quantity_delta;
    size_t set_index;
    size_t custom_die_index;
    size_t option_index;
    size_t action_index;
} dice_ui_event_t;

typedef void (*dice_ui_event_callback_t)(
    const dice_ui_event_t *event,
    void *context);

bool dice_ui_startup_begin(const char *version);
void dice_ui_startup_status(const char *message);
void dice_ui_startup_preload_set(const dice_set_definition_t *set);

bool dice_ui_start(
    dice_ui_event_callback_t callback,
    void *context,
    const dice_pool_t *pool);
void dice_ui_set_pool(const dice_pool_t *pool);
void dice_ui_wake_display(void);
bool dice_ui_is_display_dimmed(void);
void dice_ui_show_pool_cleared(void);
void dice_ui_show_notice(const char *message);
void dice_ui_show_about_status(const char *status_text);
void dice_ui_start_roll_animation(const dice_roll_result_t *result);
bool dice_ui_is_rolling(void);
bool dice_ui_is_overlay_visible(void);
void dice_ui_show_menu(bool visible);
void dice_ui_set_battery_status(
    bool available,
    uint16_t voltage_mv,
    uint8_t percent);
void dice_ui_set_menu_values(
    bool sound_enabled,
    int brightness_percent,
    int shake_level,
    int timeout_seconds,
    int selector_hide_ms);

void dice_ui_set_available_sets(
    const char *const *names,
    size_t count,
    size_t active_index);
void dice_ui_show_set_menu(bool visible);

void dice_ui_activate_standard_set(
    const dice_pool_t *pool);
void dice_ui_activate_custom_set(
    const dice_set_definition_t *set,
    const dice_custom_pool_t *pool);
void dice_ui_set_custom_pool(
    const dice_custom_pool_t *pool);
void dice_ui_start_custom_roll_animation(
    const dice_set_definition_t *set,
    const dice_custom_roll_result_t *result);

void dice_ui_start_custom_reroll_animation(
    const dice_set_definition_t *set,
    const dice_custom_roll_result_t *result,
    const bool *rerolled_mask,
    size_t rerolled_mask_size);

#define DICE_UI_MAX_RULE_OPTIONS 8

typedef struct {
    char label[32];
    char value[16];
    bool toggle;
} dice_ui_rule_option_t;

void dice_ui_set_rule_options(
    const dice_ui_rule_option_t *options,
    size_t count);
void dice_ui_show_rule_options(bool visible);

void dice_ui_set_main_rule_option(
    const char *label,
    const char *value,
    bool visible,
    size_t option_index);
void dice_ui_set_custom_result_text(
    const char *title,
    const char *details);
void dice_ui_set_custom_action(
    const char *label,
    bool visible,
    size_t action_index);
