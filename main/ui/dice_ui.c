#include "dice_ui.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bsp/esp-bsp.h"
#include "dice_audio.h"
#include "dice_face_assets.h"
#include "dice_power.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#define SCREEN_WIDTH 368
#define SCREEN_HEIGHT 448
#define TRAY_TOP 68
#define TRAY_BOTTOM 392
#define RESULT_BASE_DETAIL_LINES 2
#define RESULT_EXTRA_LINE_HEIGHT 20
#define RESULT_MAX_TRAY_TOP 148
#define SWIPE_THRESHOLD 22
#define TYPE_SLOT_WIDTH 136
#define QUANTITY_SLOT_HEIGHT 43
#define WHEEL_ITEM_COUNT 5
#define TRIPLE_TAP_WINDOW_US 900000LL
#define TAP_MAX_MOVE 42
#define CLEAR_MESSAGE_MS 1200
#define MENU_VIEWPORT_HEIGHT 350
#define MENU_CONTENT_HEIGHT 1124
#define MENU_SCROLL_TRACK_HEIGHT 350
#define MENU_SCROLL_THUMB_HEIGHT 112
#define ANIMATION_TIMER_MS 40
#define ROLL_DURATION_US 2000000LL
#define FACE_CHANGE_US 120000LL
#define COLLISION_LIMIT 12
#define SPAWN_GAP 6
#define PERCENTILE_GAP 5
#define MAX_SHAPE_POINTS 8
#define SET_MENU_MAX_SETS 16
#define SET_MENU_ROW_HEIGHT 100
#define RULE_OPTION_ROW_HEIGHT 100
#define RULE_MENU_ROW_WIDTH 270
#define RULE_MENU_ROW_HEIGHT 88
#define TOUCH_COLUMN_WIDTH (SCREEN_WIDTH / 3)
#define TOUCH_ROW_HEIGHT (SCREEN_HEIGHT / 3)
#define OPTIONS_SWIPE_THRESHOLD 44
#define MAIN_OPTION_X 206
#define MAIN_OPTION_Y 72
#define MAIN_OPTION_WIDTH 154
#define MAIN_OPTION_HEIGHT 38

static const char *TAG = "dice_ui";
static lv_display_t *s_display;
static lv_obj_t *s_startup_status_label;
static const dice_set_definition_t *s_startup_preloaded_set;

static void menu_scroll_callback(lv_event_t *event);
static void menu_scroll_anim_exec(void *var, int32_t value);
static void set_menu_swipe_callback(lv_event_t *event);
static void style_unified_menu_button(lv_obj_t *button);
static void menu_adjust_callback(lv_event_t *event);
static lv_color_t die_color(dice_type_id_t type);

typedef enum {
    GESTURE_AXIS_NONE = 0,
    GESTURE_AXIS_HORIZONTAL,
    GESTURE_AXIS_VERTICAL,
} gesture_axis_t;

typedef enum {
    TOUCH_ZONE_NONE = 0,
    TOUCH_ZONE_OPTIONS,
    TOUCH_ZONE_TYPE,
    TOUCH_ZONE_QUANTITY,
    TOUCH_ZONE_DIRECTIONAL,
} touch_zone_t;

typedef struct {
    lv_obj_t *body;
    lv_obj_t *primary_outline;
    lv_obj_t *secondary_outline;
    lv_obj_t *inner_outline;
    lv_obj_t *facet_outline_left;
    lv_obj_t *facet_outline_right;
    lv_obj_t *facet_outline_bottom;
    lv_obj_t *primary_fill;
    lv_image_dsc_t primary_fill_dsc;
    uint8_t *primary_fill_pixels;
    size_t primary_fill_pixel_count;
    lv_obj_t *secondary_fill;
    lv_image_dsc_t secondary_fill_dsc;
    uint8_t *secondary_fill_pixels;
    size_t secondary_fill_pixel_count;
    lv_obj_t *primary_label;
    lv_obj_t *secondary_label;
    lv_obj_t *primary_image;
    lv_image_dsc_t primary_image_dsc;
    lv_color32_t *primary_image_pixels;
    size_t primary_image_pixel_count;

    lv_point_precise_t primary_points[MAX_SHAPE_POINTS];
    lv_point_precise_t secondary_points[MAX_SHAPE_POINTS];
    lv_point_precise_t inner_points[MAX_SHAPE_POINTS];
    lv_point_precise_t facet_left_points[2];
    lv_point_precise_t facet_right_points[2];
    lv_point_precise_t facet_bottom_points[2];

    uint8_t primary_point_count;
    uint8_t secondary_point_count;
    uint8_t inner_point_count;

    dice_type_id_t type;
    bool custom;
    bool animated;
    size_t custom_die_index;
    size_t result_index;
    int x;
    int y;
    int vx;
    int vy;
    int width;
    int height;
    int unit_size;
} die_view_t;

static dice_ui_event_callback_t s_callback;
static void *s_callback_context;
static dice_pool_t s_pool;
static bool s_custom_mode;
static const dice_set_definition_t *s_custom_set;
static dice_custom_pool_t s_custom_pool;
static dice_custom_roll_result_t s_custom_final_result;
static int s_selected_item = DICE_D20;

static lv_obj_t *s_screen;
static lv_obj_t *s_result_label;
static lv_obj_t *s_hint_label;
static lv_obj_t *s_pool_label;
static lv_obj_t *s_selector;
static lv_obj_t *s_type_labels[WHEEL_ITEM_COUNT];
static lv_obj_t *s_quantity_labels[WHEEL_ITEM_COUNT];
static lv_obj_t *s_type_selection_box;
static lv_obj_t *s_quantity_selection_box;
static lv_obj_t *s_quantity_wheel_bar;
static lv_obj_t *s_selector_quantity_note;
static lv_obj_t *s_menu;
static lv_obj_t *s_menu_battery_label;
static lv_obj_t *s_menu_viewport;
static lv_obj_t *s_menu_content;
static lv_obj_t *s_menu_scroll_track;
static lv_obj_t *s_menu_scroll_hit_area;
static lv_obj_t *s_menu_scroll_thumb;
static lv_obj_t *s_menu_sound_label;
static lv_obj_t *s_menu_brightness_label;
static lv_obj_t *s_menu_shake_label;
static lv_obj_t *s_menu_timeout_label;
static lv_obj_t *s_menu_selector_delay_label;
static lv_obj_t *s_menu_set_label;
static lv_obj_t *s_about_panel;
static lv_obj_t *s_about_content_label;
static lv_obj_t *s_set_menu;
static lv_obj_t *s_set_menu_title;
static lv_obj_t *s_set_menu_list;
static lv_obj_t *s_set_menu_buttons[SET_MENU_MAX_SETS];
static lv_obj_t *s_set_menu_labels[SET_MENU_MAX_SETS];
static lv_obj_t *s_rule_menu;
static lv_obj_t *s_rule_menu_list;
static lv_obj_t *s_rule_clear_button;
static lv_obj_t *s_rule_option_buttons[DICE_UI_MAX_RULE_OPTIONS];
static lv_obj_t *s_rule_option_labels[DICE_UI_MAX_RULE_OPTIONS];
static lv_obj_t *s_rule_option_minus_buttons[DICE_UI_MAX_RULE_OPTIONS];
static lv_obj_t *s_rule_option_plus_buttons[DICE_UI_MAX_RULE_OPTIONS];
static dice_ui_rule_option_t s_rule_options[DICE_UI_MAX_RULE_OPTIONS];
static size_t s_rule_option_count;
static lv_obj_t *s_custom_action_button;
static lv_obj_t *s_custom_action_label;
static bool s_custom_action_visible;
static size_t s_custom_action_index;
static lv_obj_t *s_main_option_button;
static lv_obj_t *s_main_option_label;
static bool s_main_option_visible;
static char s_custom_result_title[64];
static char s_custom_result_details[96];
static char s_set_names[SET_MENU_MAX_SETS][48];
static size_t s_set_count;
static size_t s_active_set_index;
static lv_obj_t *s_clear_message;
static lv_obj_t *s_input_layer;

static lv_timer_t *s_selector_hide_timer;
static lv_timer_t *s_animation_timer;
static lv_timer_t *s_clear_message_timer;
static lv_timer_t *s_inactivity_timer;

static bool s_touching;
static touch_zone_t s_touch_zone;
static bool s_rule_menu_touching;
static lv_point_t s_rule_menu_press_point;
static bool s_set_menu_touching;
static lv_point_t s_set_menu_press_point;
static bool s_rolling;
static bool s_menu_visible;
static gesture_axis_t s_gesture_axis;
static lv_point_t s_gesture_origin;
static lv_point_t s_gesture_step_anchor;
static int s_type_drag_offset;
static int s_quantity_drag_offset;
static int64_t s_last_tap_us;
static int s_tap_count;
static lv_point_t s_press_point;
static bool s_touch_moved;

static die_view_t s_die_views[DICE_MAX_TOTAL];
static dice_roll_result_t s_final_result;
static size_t s_die_view_count;
static int64_t s_roll_started_us;
static int64_t s_last_face_change_us;
static bool s_menu_sound_enabled = true;
static int s_menu_brightness = 65;
static int s_menu_shake_level = 1;
static int s_menu_timeout_seconds = 60;
static int s_menu_selector_hide_ms = 1000;
static int s_menu_scroll_offset;
static int s_menu_scroll_start_offset;
static int s_menu_scroll_press_y;
static bool s_menu_scroll_dragging;
static int64_t s_last_activity_us;
static bool s_screen_dimmed;

static lv_obj_t *active_screen(void)
{
#if LVGL_VERSION_MAJOR >= 9
    return lv_screen_active();
#else
    return lv_scr_act();
#endif
}

static int selector_item_count(void)
{
    if (s_custom_mode &&
        s_custom_set != NULL &&
        s_custom_set->die_count > 0) {
        return (int)s_custom_set->die_count;
    }

    return DICE_TYPE_COUNT;
}

static int wrap_selector_item(int value)
{
    int count = selector_item_count();

    while (value < 0) {
        value += count;
    }
    while (value >= count) {
        value -= count;
    }
    return value;
}

static const char *selector_item_label(int item)
{
    if (s_custom_mode &&
        s_custom_set != NULL &&
        item >= 0 &&
        item < (int)s_custom_set->die_count) {
        return s_custom_set->dice[item].name;
    }

    return dice_model_type_label((dice_type_id_t)item);
}

static lv_color_t selector_item_body_color(int item)
{
    if (s_custom_mode &&
        s_custom_set != NULL &&
        item >= 0 &&
        item < (int)s_custom_set->die_count) {
        return lv_color_hex(s_custom_set->dice[item].body_color);
    }

    return die_color((dice_type_id_t)item);
}

static lv_color_t selector_item_ink_color(int item)
{
    if (s_custom_mode &&
        s_custom_set != NULL &&
        item >= 0 &&
        item < (int)s_custom_set->die_count) {
        return lv_color_hex(s_custom_set->dice[item].ink_color);
    }

    return lv_color_hex(0xffffff);
}

static void format_selector_item_label(
    const char *source,
    bool selected,
    char *destination,
    size_t destination_size)
{
    if (destination == NULL || destination_size == 0) {
        return;
    }

    destination[0] = '\0';
    if (source == NULL) {
        return;
    }

    snprintf(destination, destination_size, "%s", source);

    if (!selected) {
        char *space = strchr(destination, ' ');
        if (space != NULL) {
            *space = '\n';
        }
    }
}


static uint32_t die_color_value(dice_type_id_t type)
{
    static const uint32_t colors[DICE_TYPE_COUNT] = {
        0xe76f51,
        0xf4a261,
        0xe9c46a,
        0x2a9d8f,
        0x3ba7d8,
        0x8e7dff,
        0xc05cff,
    };
    return colors[type];
}

static lv_color_t die_color(dice_type_id_t type)
{
    return lv_color_hex(die_color_value(type));
}

static lv_color_t percentile_tens_color(void)
{
    return lv_color_hex(0x00b8d9);
}

static lv_color_t percentile_ones_color(void)
{
    return lv_color_hex(0xf062c0);
}

static lv_color_t d20_outer_color(void)
{
    return lv_color_hex(0x425aa8);
}

static void register_activity(void)
{
    s_last_activity_us = esp_timer_get_time();
    dice_power_note_activity();

    if (s_screen_dimmed) {
        bsp_display_brightness_set(s_menu_brightness);
        s_screen_dimmed = false;
    }
}

static void inactivity_timer_callback(lv_timer_t *timer)
{
    (void)timer;

    if (s_menu_timeout_seconds <= 0 || s_screen_dimmed ||
        s_rolling || s_menu_visible) {
        return;
    }

    int64_t inactive_us =
        esp_timer_get_time() - s_last_activity_us;
    if (inactive_us >=
        (int64_t)s_menu_timeout_seconds * 1000000LL) {
        bsp_display_brightness_set(0);
        s_screen_dimmed = true;
    }
}

static void update_pool_text(void)
{
    char text[144] = {0};
    size_t used = 0;

    if (s_custom_mode && s_custom_set != NULL) {
        for (size_t index = 0;
             index < s_custom_set->die_count;
             ++index) {
            uint16_t quantity =
                s_custom_pool.quantity[index];

            if (quantity == 0) {
                continue;
            }

            int written = snprintf(
                text + used,
                sizeof(text) - used,
                "%s%u %s",
                used == 0 ? "" : "  ",
                (unsigned)quantity,
                s_custom_set->dice[index].name);

            if (written < 0 ||
                (size_t)written >=
                    sizeof(text) - used) {
                break;
            }

            used += (size_t)written;
        }
    } else {
        for (int type = 0;
             type < DICE_TYPE_COUNT;
             ++type) {
            uint16_t quantity =
                s_pool.quantity[type];

            if (quantity == 0) {
                continue;
            }

            int written = snprintf(
                text + used,
                sizeof(text) - used,
                "%s%u%s",
                used == 0 ? "" : "  ",
                (unsigned)quantity,
                dice_model_type_label(
                    (dice_type_id_t)type));

            if (written < 0 ||
                (size_t)written >=
                    sizeof(text) - used) {
                break;
            }

            used += (size_t)written;
        }
    }

    if (used == 0) {
        snprintf(text, sizeof(text), "No dice selected");
    }

    lv_label_set_text(s_pool_label, text);
}

static void position_selector_wheels(void)
{
    lv_obj_update_layout(s_selector);
    const int type_center_x =
        lv_obj_get_x(s_type_selection_box) +
        lv_obj_get_width(s_type_selection_box) / 2;
    const int type_center_y = 66;
    const int quantity_center_x = 167;
    const int quantity_center_y = 206;

    for (int index = 0; index < WHEEL_ITEM_COUNT; ++index) {
        int relative = index - 2;
        int item = wrap_selector_item(s_selected_item + relative);

        char type_text[DICE_SET_NAME_MAX + 2];
        format_selector_item_label(
            selector_item_label(item),
            relative == 0,
            type_text,
            sizeof(type_text));
        lv_label_set_text(
            s_type_labels[index],
            type_text);

        int label_width =
            relative == 0 ? 146 : 104;
        int label_height =
            relative == 0 ? 58 : 48;
        lv_obj_set_size(
            s_type_labels[index],
            label_width,
            label_height);
        lv_obj_set_pos(
            s_type_labels[index],
            type_center_x - label_width / 2 +
                relative * TYPE_SLOT_WIDTH +
                s_type_drag_offset,
            type_center_y - label_height / 2);
        lv_obj_set_style_text_font(
            s_type_labels[index],
            relative == 0
                ? &lv_font_montserrat_18
                : &lv_font_montserrat_14,
            LV_PART_MAIN);
        lv_obj_set_style_text_color(
            s_type_labels[index],
            selector_item_ink_color(item),
            LV_PART_MAIN);
        lv_obj_set_style_bg_color(
            s_type_labels[index],
            selector_item_body_color(item),
            LV_PART_MAIN);
        lv_obj_set_style_bg_opa(
            s_type_labels[index],
            relative == 0 ? LV_OPA_COVER : LV_OPA_80,
            LV_PART_MAIN);
        lv_obj_set_style_radius(
            s_type_labels[index],
            8,
            LV_PART_MAIN);
        lv_obj_set_style_pad_left(
            s_type_labels[index],
            4,
            LV_PART_MAIN);
        lv_obj_set_style_pad_right(
            s_type_labels[index],
            4,
            LV_PART_MAIN);
        lv_obj_set_style_pad_top(
            s_type_labels[index],
            4,
            LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(
            s_type_labels[index],
            4,
            LV_PART_MAIN);
    }

    int selected_quantity =
        s_custom_mode
            ? (int)dice_custom_pool_get(
                  &s_custom_pool,
                  (size_t)s_selected_item)
            : (int)dice_model_get_quantity(
                  &s_pool,
                  (dice_type_id_t)s_selected_item);

    for (int index = 0; index < WHEEL_ITEM_COUNT; ++index) {
        int relative = index - 2;
        int quantity = selected_quantity + relative;

        if (!s_custom_mode &&
        s_selected_item == DICE_PERCENTILE) {
            if (quantity < 0) {
                quantity = 0;
            }
            if (quantity > 1) {
                quantity = 1;
            }
        } else if (quantity < 0) {
            quantity = 0;
        }

        char number_text[12];
        snprintf(number_text, sizeof(number_text), "%d", quantity);
        lv_label_set_text(s_quantity_labels[index], number_text);
        lv_obj_set_width(s_quantity_labels[index], 72);
        lv_obj_set_pos(
            s_quantity_labels[index],
            quantity_center_x - 36,
            quantity_center_y - 17 +
                relative * QUANTITY_SLOT_HEIGHT +
                s_quantity_drag_offset);
        lv_obj_set_style_text_color(
            s_quantity_labels[index],
            relative == 0
                ? lv_color_hex(0xffdc68)
                : lv_color_hex(0x8f8050),
            LV_PART_MAIN);
    }

    lv_obj_clear_flag(
        s_quantity_selection_box,
        LV_OBJ_FLAG_HIDDEN);
    for (int index = 0; index < WHEEL_ITEM_COUNT; ++index) {
        lv_obj_clear_flag(
            s_quantity_labels[index],
            LV_OBJ_FLAG_HIDDEN);
    }

    if (!s_custom_mode &&
        s_selected_item == DICE_PERCENTILE) {
        lv_label_set_text(
            s_selector_quantity_note,
            "Percentile pair: quantity 0 or 1");
    } else {
        lv_label_set_text(s_selector_quantity_note, "");
    }
}

static void update_selector_text(void)
{
    s_type_drag_offset = 0;
    s_quantity_drag_offset = 0;
    position_selector_wheels();
}

static void cancel_selector_hide(void)
{
    if (s_selector_hide_timer != NULL) {
        lv_timer_delete(s_selector_hide_timer);
        s_selector_hide_timer = NULL;
    }
}

static void selector_hide_callback(lv_timer_t *timer)
{
    (void)timer;
    s_selector_hide_timer = NULL;

    if (!s_touching && !s_rolling && !s_menu_visible) {
        lv_obj_add_flag(s_selector, LV_OBJ_FLAG_HIDDEN);
    }
}

static void schedule_selector_hide(void)
{
    cancel_selector_hide();
    s_selector_hide_timer = lv_timer_create(
        selector_hide_callback,
        (uint32_t)s_menu_selector_hide_ms,
        NULL);
    lv_timer_set_repeat_count(s_selector_hide_timer, 1);
}

static void place_overlay_above_dice(lv_obj_t *overlay)
{
    lv_obj_move_foreground(overlay);
    lv_obj_move_foreground(s_input_layer);
}

static void show_selector(void)
{
    cancel_selector_hide();
    update_selector_text();
    lv_obj_clear_flag(s_selector, LV_OBJ_FLAG_HIDDEN);
    place_overlay_above_dice(s_selector);
}

static void emit_quantity_delta(int delta)
{
    if (s_callback == NULL) {
        return;
    }

    const dice_ui_event_t event = {
        .type = DICE_UI_EVENT_SELECTOR_CHANGED,
        .selected_type = (dice_type_id_t)s_selected_item,
        .quantity_delta = delta,
        .custom_die_index =
            s_custom_mode
                ? (size_t)s_selected_item
                : 0,
    };
    s_callback(&event, s_callback_context);
}

static void emit_simple_event(dice_ui_event_type_t type)
{
    if (s_callback == NULL) {
        return;
    }

    const dice_ui_event_t event = {
        .type = type,
        .selected_type = DICE_D4,
        .quantity_delta = 0,
    };
    s_callback(&event, s_callback_context);
}

static void commit_type_step(int direction)
{
    s_selected_item = wrap_selector_item(
        s_selected_item + direction);
    s_type_drag_offset = 0;
    s_quantity_drag_offset = 0;
    position_selector_wheels();
    dice_audio_play_tick();
}

static void commit_quantity_step(int direction)
{
    bool changed = false;

    if (s_custom_mode) {
        uint16_t before = dice_custom_pool_get(
            &s_custom_pool,
            (size_t)s_selected_item);

        dice_custom_pool_adjust(
            &s_custom_pool,
            (size_t)s_selected_item,
            direction);

        uint16_t after = dice_custom_pool_get(
            &s_custom_pool,
            (size_t)s_selected_item);
        changed = before != after;
    } else {
        dice_type_id_t type =
            (dice_type_id_t)s_selected_item;
        uint16_t before =
            dice_model_get_quantity(&s_pool, type);

        dice_model_adjust_quantity(
            &s_pool,
            type,
            direction);

        uint16_t after =
            dice_model_get_quantity(&s_pool, type);
        changed = before != after;
    }

    if (changed) {
        emit_quantity_delta(direction);
        dice_audio_play_tick();
        update_pool_text();
    }

    s_quantity_drag_offset = 0;
    position_selector_wheels();
}

static touch_zone_t touch_zone_for_point(
    const lv_point_t *point)
{
    if (point == NULL) {
        return TOUCH_ZONE_NONE;
    }

    bool in_right_column =
        point->x >=
            SCREEN_WIDTH - TOUCH_COLUMN_WIDTH;
    bool in_bottom_row =
        point->y >=
            SCREEN_HEIGHT - TOUCH_ROW_HEIGHT;

    if (in_right_column && in_bottom_row) {
        return TOUCH_ZONE_DIRECTIONAL;
    }

    if (in_right_column) {
        return TOUCH_ZONE_QUANTITY;
    }

    if (in_bottom_row) {
        return TOUCH_ZONE_TYPE;
    }

    if (point->x < TOUCH_COLUMN_WIDTH &&
        point->y < TOUCH_ROW_HEIGHT) {
        return TOUCH_ZONE_OPTIONS;
    }

    return TOUCH_ZONE_NONE;
}

static void open_roll_options(void)
{
    /*
     * Roll Options is universal because Clear dice pool is always present,
     * even when the active template defines no custom controls or actions.
     */
    dice_ui_show_rule_options(true);
}

static void touch_callback(lv_event_t *event)
{
    if (s_rolling || s_menu_visible) {
        return;
    }

    lv_indev_t *input_device = lv_indev_active();
    if (input_device == NULL) {
        return;
    }

    lv_point_t point = {0};
    lv_indev_get_point(input_device, &point);
    lv_event_code_t code = lv_event_get_code(event);

    if (code == LV_EVENT_PRESSED) {
        register_activity();
        dice_power_high_performance_acquire();

        s_touching = true;
        s_touch_moved = false;
        s_press_point = point;
        s_gesture_axis = GESTURE_AXIS_NONE;
        s_gesture_origin = point;
        s_gesture_step_anchor = point;
        s_type_drag_offset = 0;
        s_quantity_drag_offset = 0;
        s_touch_zone = touch_zone_for_point(&point);

        lv_obj_add_flag(
            s_quantity_wheel_bar,
            LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (code == LV_EVENT_RELEASED) {
        dice_power_high_performance_release();

        bool open_options =
            s_touching &&
            s_touch_zone == TOUCH_ZONE_OPTIONS &&
            !s_touch_moved;

        s_touching = false;
        s_touch_zone = TOUCH_ZONE_NONE;
        s_gesture_axis = GESTURE_AXIS_NONE;
        s_type_drag_offset = 0;
        s_quantity_drag_offset = 0;

        lv_obj_add_flag(
            s_quantity_wheel_bar,
            LV_OBJ_FLAG_HIDDEN);
        position_selector_wheels();

        if (open_options) {
            open_roll_options();
            return;
        }

        bool in_sector_five =
            s_press_point.x >= TOUCH_COLUMN_WIDTH &&
            s_press_point.x <
                SCREEN_WIDTH - TOUCH_COLUMN_WIDTH &&
            s_press_point.y >= TOUCH_ROW_HEIGHT &&
            s_press_point.y <
                SCREEN_HEIGHT - TOUCH_ROW_HEIGHT;

        if (!s_touch_moved && in_sector_five) {
            int64_t now_us = esp_timer_get_time();

            if (now_us - s_last_tap_us <=
                TRIPLE_TAP_WINDOW_US) {
                ++s_tap_count;
            } else {
                s_tap_count = 1;
            }

            s_last_tap_us = now_us;

            if (s_tap_count >= 3) {
                s_tap_count = 0;
                emit_simple_event(
                    DICE_UI_EVENT_CLEAR_POOL);
                return;
            }
        } else {
            s_tap_count = 0;
        }

        if (!lv_obj_has_flag(
                s_selector,
                LV_OBJ_FLAG_HIDDEN)) {
            schedule_selector_hide();
        }
        return;
    }

    if (code != LV_EVENT_PRESSING ||
        !s_touching) {
        return;
    }

    int dx = point.x - s_gesture_origin.x;
    int dy = point.y - s_gesture_origin.y;

    if (abs(point.x - s_press_point.x) >
            TAP_MAX_MOVE ||
        abs(point.y - s_press_point.y) >
            TAP_MAX_MOVE) {
        s_touch_moved = true;
    }

    if (s_touch_zone == TOUCH_ZONE_OPTIONS ||
        s_touch_zone == TOUCH_ZONE_NONE) {
        return;
    }

    if (s_gesture_axis == GESTURE_AXIS_NONE) {
        if (s_touch_zone == TOUCH_ZONE_TYPE) {
            if (abs(dx) < SWIPE_THRESHOLD) {
                return;
            }

            s_gesture_axis =
                GESTURE_AXIS_HORIZONTAL;
        } else if (
            s_touch_zone ==
                TOUCH_ZONE_QUANTITY) {
            if (abs(dy) < SWIPE_THRESHOLD) {
                return;
            }

            s_gesture_axis =
                GESTURE_AXIS_VERTICAL;
        } else if (
            s_touch_zone ==
                TOUCH_ZONE_DIRECTIONAL) {
            if (abs(dx) < SWIPE_THRESHOLD &&
                abs(dy) < SWIPE_THRESHOLD) {
                return;
            }

            s_gesture_axis =
                abs(dx) >= abs(dy)
                    ? GESTURE_AXIS_HORIZONTAL
                    : GESTURE_AXIS_VERTICAL;
        }

        show_selector();

        if (s_gesture_axis ==
            GESTURE_AXIS_VERTICAL) {
            lv_obj_clear_flag(
                s_quantity_wheel_bar,
                LV_OBJ_FLAG_HIDDEN);
        }

        s_gesture_step_anchor = point;
        return;
    }

    if (s_gesture_axis ==
        GESTURE_AXIS_HORIZONTAL) {
        int delta_x =
            point.x -
            s_gesture_step_anchor.x;

        /* Two pixels of carousel travel per pixel of finger movement. */
        s_type_drag_offset += delta_x * 2;
        s_gesture_step_anchor = point;

        while (s_type_drag_offset <=
               -TYPE_SLOT_WIDTH) {
            s_type_drag_offset +=
                TYPE_SLOT_WIDTH;
            commit_type_step(1);
        }

        while (s_type_drag_offset >=
               TYPE_SLOT_WIDTH) {
            s_type_drag_offset -=
                TYPE_SLOT_WIDTH;
            commit_type_step(-1);
        }

        position_selector_wheels();
        return;
    }

    int delta_y =
        point.y -
        s_gesture_step_anchor.y;

    s_quantity_drag_offset += delta_y;
    s_gesture_step_anchor = point;

    while (s_quantity_drag_offset <=
           -QUANTITY_SLOT_HEIGHT) {
        s_quantity_drag_offset +=
            QUANTITY_SLOT_HEIGHT;
        commit_quantity_step(1);
    }

    while (s_quantity_drag_offset >=
           QUANTITY_SLOT_HEIGHT) {
        s_quantity_drag_offset -=
            QUANTITY_SLOT_HEIGHT;
        commit_quantity_step(-1);
    }

    position_selector_wheels();
}

static void refresh_after_hiding_overlay(void)
{
    /*
     * Flush the hidden selector/menu state while internal DMA memory is still
     * plentiful. This prevents stale selector pixels remaining underneath a
     * roll if later rendering encounters memory pressure.
     */
    lv_obj_invalidate(s_screen);
    lv_refr_now(NULL);
}

static void clear_die_views(void)
{
    for (size_t index = 0; index < DICE_MAX_TOTAL; ++index) {
        if (s_die_views[index].primary_image_pixels != NULL) {
            free(s_die_views[index].primary_image_pixels);
            s_die_views[index].primary_image_pixels = NULL;
            s_die_views[index].primary_image_pixel_count = 0;
        }
        if (s_die_views[index].primary_fill_pixels != NULL) {
            heap_caps_free(s_die_views[index].primary_fill_pixels);
            s_die_views[index].primary_fill_pixels = NULL;
            s_die_views[index].primary_fill_pixel_count = 0;
        }
        if (s_die_views[index].secondary_fill_pixels != NULL) {
            heap_caps_free(s_die_views[index].secondary_fill_pixels);
            s_die_views[index].secondary_fill_pixels = NULL;
            s_die_views[index].secondary_fill_pixel_count = 0;
        }
        if (s_die_views[index].body != NULL) {
            lv_obj_delete(s_die_views[index].body);
        }
        memset(&s_die_views[index], 0, sizeof(s_die_views[index]));
    }

    s_die_view_count = 0;
}

static void set_point(
    lv_point_precise_t *point,
    int x,
    int y)
{
    point->x = x;
    point->y = y;
}

static uint8_t build_shape_points(
    dice_type_id_t type,
    int origin_x,
    int origin_y,
    int size,
    lv_point_precise_t *points)
{
    int left = origin_x + 2;
    int top = origin_y + 2;
    int right = origin_x + size - 3;
    int bottom = origin_y + size - 3;
    int center_x = origin_x + size / 2;
    int center_y = origin_y + size / 2;

    switch (type) {
        case DICE_D4:
            set_point(&points[0], center_x, top);
            set_point(&points[1], right, bottom);
            set_point(&points[2], left, bottom);
            set_point(&points[3], center_x, top);
            return 4;

        case DICE_D6:
            set_point(&points[0], left, top);
            set_point(&points[1], right, top);
            set_point(&points[2], right, bottom);
            set_point(&points[3], left, bottom);
            set_point(&points[4], left, top);
            return 5;

        case DICE_D8:
            set_point(&points[0], left, top);
            set_point(&points[1], right, top);
            set_point(&points[2], center_x, bottom);
            set_point(&points[3], left, top);
            return 4;

        case DICE_D10:
        case DICE_PERCENTILE:
            set_point(&points[0], center_x, top);
            set_point(&points[1], right, center_y);
            set_point(&points[2], center_x, bottom);
            set_point(&points[3], left, center_y);
            set_point(&points[4], center_x, top);
            return 5;

        case DICE_D12:
            set_point(&points[0], center_x, top);
            set_point(&points[1], right, origin_y + size * 38 / 100);
            set_point(&points[2], origin_x + size * 79 / 100, bottom);
            set_point(&points[3], origin_x + size * 21 / 100, bottom);
            set_point(&points[4], left, origin_y + size * 38 / 100);
            set_point(&points[5], center_x, top);
            return 6;

        case DICE_D20:
            set_point(&points[0], center_x, top);
            set_point(&points[1], right, origin_y + size / 4);
            set_point(&points[2], right, origin_y + size * 3 / 4);
            set_point(&points[3], center_x, bottom);
            set_point(&points[4], left, origin_y + size * 3 / 4);
            set_point(&points[5], left, origin_y + size / 4);
            set_point(&points[6], center_x, top);
            return 7;
    }

    return 0;
}

static lv_obj_t *create_outline(
    lv_obj_t *parent,
    lv_point_precise_t *points,
    uint8_t point_count,
    lv_color_t color,
    int line_width)
{
    lv_obj_t *line = lv_line_create(parent);
    lv_line_set_points(line, points, point_count);
    lv_obj_set_style_line_color(line, color, LV_PART_MAIN);
    lv_obj_set_style_line_width(line, line_width, LV_PART_MAIN);
    lv_obj_set_style_line_rounded(line, true, LV_PART_MAIN);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);
    return line;
}

static void style_value_label(lv_obj_t *label, int size)
{
    lv_obj_set_style_text_color(
        label, lv_color_hex(0xffffff), LV_PART_MAIN);

    if (size >= 54) {
        lv_obj_set_style_text_font(
            label, &lv_font_montserrat_24, LV_PART_MAIN);
    } else {
        lv_obj_set_style_text_font(
            label, &lv_font_montserrat_18, LV_PART_MAIN);
    }

    lv_obj_set_style_text_align(
        label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
}

static bool ensure_polygon_fill(
    die_view_t *view,
    bool secondary,
    uint32_t body_color);

static void create_die_graphics(
    die_view_t *view,
    int unit_size,
    dice_type_id_t type)
{
    memset(view->primary_points, 0, sizeof(view->primary_points));
    memset(view->secondary_points, 0, sizeof(view->secondary_points));
    memset(view->inner_points, 0, sizeof(view->inner_points));
    memset(view->facet_left_points, 0, sizeof(view->facet_left_points));
    memset(view->facet_right_points, 0, sizeof(view->facet_right_points));
    memset(view->facet_bottom_points, 0, sizeof(view->facet_bottom_points));

    view->type = type;
    view->unit_size = unit_size;
    view->height = unit_size;

    if (type == DICE_PERCENTILE) {
        view->width = unit_size * 2 + PERCENTILE_GAP;
    } else {
        view->width = unit_size;
    }

    view->body = lv_obj_create(s_screen);
    lv_obj_set_size(view->body, view->width, view->height);
    lv_obj_set_style_bg_opa(
        view->body, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(
        view->body, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(
        view->body, 0, LV_PART_MAIN);
    lv_obj_clear_flag(
        view->body, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    if (type == DICE_PERCENTILE) {
        view->primary_point_count = build_shape_points(
            DICE_PERCENTILE,
            0,
            0,
            unit_size,
            view->primary_points);
        view->secondary_point_count = build_shape_points(
            DICE_PERCENTILE,
            unit_size + PERCENTILE_GAP,
            0,
            unit_size,
            view->secondary_points);

        view->primary_outline = create_outline(
            view->body,
            view->primary_points,
            view->primary_point_count,
            percentile_tens_color(),
            4);
        view->secondary_outline = create_outline(
            view->body,
            view->secondary_points,
            view->secondary_point_count,
            percentile_ones_color(),
            4);

        ensure_polygon_fill(
            view,
            false,
            0x00b8d9);
        ensure_polygon_fill(
            view,
            true,
            0xf062c0);

        view->primary_label = lv_label_create(view->body);
        view->secondary_label = lv_label_create(view->body);
        style_value_label(view->primary_label, unit_size);
        style_value_label(view->secondary_label, unit_size);

        lv_obj_set_width(view->primary_label, unit_size);
        lv_obj_set_width(view->secondary_label, unit_size);
        lv_obj_set_pos(
            view->primary_label,
            0,
            unit_size / 2 - (unit_size >= 54 ? 15 : 11));
        lv_obj_set_pos(
            view->secondary_label,
            unit_size + PERCENTILE_GAP,
            unit_size / 2 - (unit_size >= 54 ? 15 : 11));
        return;
    }

    view->primary_point_count = build_shape_points(
        type,
        0,
        0,
        unit_size,
        view->primary_points);
    view->primary_outline = create_outline(
        view->body,
        view->primary_points,
        view->primary_point_count,
        type == DICE_D20 ? d20_outer_color() : die_color(type),
        type == DICE_D20 ? 5 : 4);

    ensure_polygon_fill(
        view,
        false,
        die_color_value(type));

    view->primary_label = lv_label_create(view->body);
    style_value_label(view->primary_label, unit_size);
    lv_obj_set_width(view->primary_label, unit_size);
    lv_obj_set_pos(
        view->primary_label,
        0,
        unit_size / 2 - (unit_size >= 54 ? 15 : 11));
}

static void set_die_face(
    die_view_t *view,
    const die_result_t *face)
{
    if (view == NULL || face == NULL) {
        return;
    }

    char primary[12];

    if (face->kind == DIE_RESULT_PERCENTILE) {
        snprintf(primary, sizeof(primary), "%02d", face->display_primary);
        lv_label_set_text(view->primary_label, primary);

        char secondary[8];
        snprintf(
            secondary,
            sizeof(secondary),
            "%d",
            face->display_secondary);
        lv_label_set_text(view->secondary_label, secondary);
        return;
    }

    snprintf(primary, sizeof(primary), "%d", face->display_primary);
    lv_label_set_text(view->primary_label, primary);
}


static dice_type_id_t custom_shape_type(
    const char *shape)
{
    if (shape == NULL) {
        return DICE_D6;
    }

    if (strcmp(shape, "d4") == 0 ||
        strcmp(shape, "triangle_up") == 0) {
        return DICE_D4;
    }
    if (strcmp(shape, "d8") == 0 ||
        strcmp(shape, "triangle_down") == 0) {
        return DICE_D8;
    }
    if (strcmp(shape, "d10") == 0 ||
        strcmp(shape, "diamond") == 0) {
        return DICE_D10;
    }
    if (strcmp(shape, "d12") == 0 ||
        strcmp(shape, "pentagon") == 0) {
        return DICE_D12;
    }
    if (strcmp(shape, "d20") == 0 ||
        strcmp(shape, "hexagon") == 0) {
        return DICE_D20;
    }

    return DICE_D6;
}


static int custom_content_y_offset(
    dice_type_id_t type,
    int unit_size)
{
    if (type == DICE_D8) {
        /* Down-pointing triangle: keep content in the wide upper section. */
        return -(unit_size * 11 / 54);
    }

    if (type == DICE_D4) {
        /* Up-pointing triangle: keep content in the wide lower section. */
        return unit_size * 10 / 54;
    }

    return 0;
}


static bool point_inside_polygon(
    int px,
    int py,
    const lv_point_precise_t *points,
    uint8_t point_count)
{
    if (points == NULL || point_count < 3) {
        return false;
    }

    /* build_shape_points repeats the first point as the final point. */
    int vertex_count = point_count;
    if (point_count > 3 &&
        points[0].x == points[point_count - 1].x &&
        points[0].y == points[point_count - 1].y) {
        vertex_count--;
    }

    bool inside = false;
    int previous = vertex_count - 1;

    for (int current = 0; current < vertex_count; ++current) {
        int x1 = (int)points[current].x;
        int y1 = (int)points[current].y;
        int x2 = (int)points[previous].x;
        int y2 = (int)points[previous].y;

        bool crosses = ((y1 > py) != (y2 > py));
        if (crosses) {
            int64_t numerator =
                (int64_t)(x2 - x1) * (int64_t)(py - y1);
            int denominator = y2 - y1;
            int intersection_x = x1 + (int)(numerator / denominator);

            if (px < intersection_x) {
                inside = !inside;
            }
        }

        previous = current;
    }

    return inside;
}



static bool ensure_polygon_fill(
    die_view_t *view,
    bool secondary,
    uint32_t body_color)
{
    if (view == NULL || view->height <= 0 || view->width <= 0) {
        return false;
    }

    const lv_point_precise_t *points = secondary
        ? view->secondary_points
        : view->primary_points;
    uint8_t point_count = secondary
        ? view->secondary_point_count
        : view->primary_point_count;

    if (point_count < 3) {
        return false;
    }

    size_t required_count =
        (size_t)view->width * (size_t)view->height;
    uint8_t **pixels = secondary
        ? &view->secondary_fill_pixels
        : &view->primary_fill_pixels;
    size_t *pixel_count = secondary
        ? &view->secondary_fill_pixel_count
        : &view->primary_fill_pixel_count;
    lv_image_dsc_t *descriptor = secondary
        ? &view->secondary_fill_dsc
        : &view->primary_fill_dsc;
    lv_obj_t **image = secondary
        ? &view->secondary_fill
        : &view->primary_fill;

    if (*pixels == NULL || *pixel_count != required_count) {
        heap_caps_free(*pixels);
        *pixels = heap_caps_malloc(
            required_count,
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

        if (*pixels == NULL) {
            *pixels = heap_caps_malloc(
                required_count,
                MALLOC_CAP_8BIT);
        }

        if (*pixels == NULL) {
            *pixel_count = 0;
            ESP_LOGW(TAG, "Could not allocate die alpha mask");
            return false;
        }

        *pixel_count = required_count;
    }

    for (int y = 0; y < view->height; ++y) {
        for (int x = 0; x < view->width; ++x) {
            bool inside = point_inside_polygon(
                x,
                y,
                points,
                point_count);
            (*pixels)[
                (size_t)y * (size_t)view->width + (size_t)x] =
                inside ? 255U : 0U;
        }
    }

    memset(descriptor, 0, sizeof(*descriptor));
    descriptor->header.cf = LV_COLOR_FORMAT_A8;
    descriptor->header.w = view->width;
    descriptor->header.h = view->height;
    descriptor->header.stride = view->width;
    descriptor->data_size = (uint32_t)required_count;
    descriptor->data = *pixels;

    if (*image == NULL) {
        *image = lv_image_create(view->body);
        lv_obj_clear_flag(
            *image,
            LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    }

    lv_image_set_src(*image, descriptor);
    lv_obj_set_style_image_recolor(
        *image,
        lv_color_hex(body_color),
        LV_PART_MAIN);
    lv_obj_set_style_image_recolor_opa(
        *image,
        LV_OPA_COVER,
        LV_PART_MAIN);
    lv_obj_set_pos(*image, 0, 0);
    lv_obj_remove_flag(*image, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_background(*image);
    return true;
}

static void style_custom_face_label(
    lv_obj_t *label,
    const char *text,
    int unit_size,
    dice_type_id_t type)
{
    size_t length =
        text != NULL ? strlen(text) : 0;

    const lv_font_t *font =
        &lv_font_montserrat_18;

    if (unit_size >= 54 && length <= 2) {
        font = &lv_font_montserrat_24;
    } else if (length <= 4) {
        font = &lv_font_montserrat_18;
    } else if (length <= 7) {
        font = &lv_font_montserrat_14;
    } else {
        font = &lv_font_montserrat_12;
    }

    lv_obj_set_style_text_font(
        label,
        font,
        LV_PART_MAIN);
    lv_obj_set_style_text_align(
        label,
        LV_TEXT_ALIGN_CENTER,
        LV_PART_MAIN);
    lv_label_set_long_mode(
        label,
        LV_LABEL_LONG_CLIP);

    int inset = 8;
    int width = unit_size - inset * 2;
    if (width < 12) {
        width = 12;
    }

    lv_obj_set_width(label, width);
    lv_obj_set_pos(
        label,
        inset,
        unit_size / 2 -
            (font->line_height / 2) +
            custom_content_y_offset(type, unit_size));
}


static void clear_custom_face_image(die_view_t *view)
{
    if (view == NULL) {
        return;
    }

    if (view->primary_image != NULL) {
        lv_obj_add_flag(
            view->primary_image,
            LV_OBJ_FLAG_HIDDEN);
    }
}


static int custom_face_image_size(int unit_size)
{
    if (unit_size >= 54) {
        return 48;
    }
    if (unit_size >= 44) {
        return 38;
    }
    if (unit_size >= 38) {
        return 32;
    }
    return 28;
}

static bool render_custom_face_image(
    die_view_t *view,
    const dice_set_definition_t *set,
    const dice_set_face_t *face)
{
    if (view == NULL ||
        set == NULL ||
        face == NULL ||
        face->image_path[0] == '\0') {
        return false;
    }

    char absolute_path[DICE_SET_PATH_MAX * 2 + 4];
    int written = snprintf(
        absolute_path,
        sizeof(absolute_path),
        "%s/%s",
        set->folder_path,
        face->image_path);

    if (written <= 0 || written >= (int)sizeof(absolute_path)) {
        ESP_LOGW(TAG, "Image path is too long for face asset");
        return false;
    }

    dice_face_art_mode_t effective_mode =
        face->art_mode == DICE_FACE_ART_INDEXED
            ? DICE_FACE_ART_INDEXED
            : DICE_FACE_ART_MASK;

    int image_size =
        custom_face_image_size(view->unit_size);

    const lv_image_dsc_t *cached_image = NULL;
    if (!dice_face_assets_get_rendered(
            absolute_path,
            effective_mode,
            face->ink_color,
            image_size,
            image_size,
            &cached_image) ||
        cached_image == NULL) {
        return false;
    }

    view->primary_image_dsc = *cached_image;

    if (view->primary_image == NULL) {
        view->primary_image = lv_image_create(view->body);
        lv_obj_clear_flag(
            view->primary_image,
            LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    }

    lv_image_set_src(
        view->primary_image,
        &view->primary_image_dsc);
    /*
     * PNG overlays are authored against the complete 48x48 face canvas.
     * Keep them geometrically centered; triangle-specific offsets apply only
     * to generated text labels.
     */
    lv_obj_set_pos(
        view->primary_image,
        (view->width - image_size) / 2,
        (view->height - image_size) / 2);
    lv_obj_remove_flag(
        view->primary_image,
        LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(view->primary_image);
    return true;
}

static void apply_custom_face(
    die_view_t *view,
    const dice_set_definition_t *set,
    const dice_set_die_t *die,
    const dice_set_face_t *face)
{
    if (view == NULL || set == NULL || die == NULL || face == NULL) {
        return;
    }

    lv_color_t ink_color =
        lv_color_hex(face->ink_color);

    ensure_polygon_fill(
        view,
        false,
        face->body_color);

    lv_obj_set_style_text_color(
        view->primary_label,
        ink_color,
        LV_PART_MAIN);

    clear_custom_face_image(view);
    lv_obj_add_flag(
        view->primary_label,
        LV_OBJ_FLAG_HIDDEN);

    char text[40];
    text[0] = '\0';

    switch (face->display_mode) {
        case DICE_FACE_DISPLAY_IMAGE:
            if (face->image_path[0] != '\0' &&
                render_custom_face_image(
                    view,
                    set,
                    face)) {
                return;
            }
            break;

        case DICE_FACE_DISPLAY_LABEL:
            if (face->label[0] != '\0') {
                snprintf(
                    text,
                    sizeof(text),
                    "%.31s",
                    face->label);
            }
            break;

        case DICE_FACE_DISPLAY_VALUE:
            if (face->has_numeric_value) {
                snprintf(
                    text,
                    sizeof(text),
                    "%ld",
                    (long)face->numeric_value);
            }
            break;

        case DICE_FACE_DISPLAY_BLANK:
        default:
            break;
    }

    if (text[0] == '\0') {
        return;
    }

    lv_obj_remove_flag(
        view->primary_label,
        LV_OBJ_FLAG_HIDDEN);
    style_custom_face_label(
        view->primary_label,
        text,
        view->unit_size,
        view->type);
    lv_label_set_text(
        view->primary_label,
        text);
}

static void create_custom_die_graphics(
    die_view_t *view,
    int unit_size,
    size_t die_index,
    const dice_set_die_t *die)
{
    dice_type_id_t shape_type =
        custom_shape_type(die->shape);

    create_die_graphics(
        view,
        unit_size,
        shape_type);

    view->custom = true;
    view->custom_die_index = die_index;

    /*
     * Render a transparent ARGB image whose opaque pixels match the polygon.
     * This gives custom dice a solid body without reverting to a square fill.
     */
    ensure_polygon_fill(
        view,
        false,
        die->body_color);

    /*
     * Custom dice use only the filled polygon. Hiding the outline prevents
     * polygon edges from being drawn over face text or PNG artwork.
     */
    if (view->primary_outline != NULL) {
        lv_obj_add_flag(
            view->primary_outline,
            LV_OBJ_FLAG_HIDDEN);
    }
    if (view->secondary_outline != NULL) {
        lv_obj_add_flag(
            view->secondary_outline,
            LV_OBJ_FLAG_HIDDEN);
    }
    if (view->inner_outline != NULL) {
        lv_obj_add_flag(
            view->inner_outline,
            LV_OBJ_FLAG_HIDDEN);
    }
    if (view->facet_outline_left != NULL) {
        lv_obj_add_flag(
            view->facet_outline_left,
            LV_OBJ_FLAG_HIDDEN);
    }
    if (view->facet_outline_right != NULL) {
        lv_obj_add_flag(
            view->facet_outline_right,
            LV_OBJ_FLAG_HIDDEN);
    }
    if (view->facet_outline_bottom != NULL) {
        lv_obj_add_flag(
            view->facet_outline_bottom,
            LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_move_foreground(view->primary_label);

    lv_obj_set_style_text_color(
        view->primary_label,
        lv_color_hex(die->ink_color),
        LV_PART_MAIN);
}


static int animation_unit_size(size_t physical_die_count)
{
    /* Keep the most readable full-size faces through three rows. */
    if (physical_die_count <= 18) {
        return 54;
    }
    if (physical_die_count <= 24) {
        return 44;
    }
    if (physical_die_count <= 32) {
        return 38;
    }
    return 34;
}

static bool rectangles_overlap(
    const die_view_t *a,
    const die_view_t *b,
    int gap)
{
    return a->x < b->x + b->width + gap &&
           a->x + a->width + gap > b->x &&
           a->y < b->y + b->height + gap &&
           a->y + a->height + gap > b->y;
}

static void choose_spawn_position(
    die_view_t *view,
    size_t existing_count)
{
    int max_x = SCREEN_WIDTH - view->width - 3;
    int max_y = TRAY_BOTTOM - view->height - 3;

    for (int attempt = 0; attempt < 80; ++attempt) {
        view->x = 3 + (int)(esp_random() % (uint32_t)(max_x - 2));
        view->y = TRAY_TOP +
            (int)(esp_random() % (uint32_t)(max_y - TRAY_TOP + 1));

        bool overlap = false;
        for (size_t previous = 0; previous < existing_count; ++previous) {
            if (rectangles_overlap(
                    view, &s_die_views[previous], SPAWN_GAP)) {
                overlap = true;
                break;
            }
        }

        if (!overlap) {
            return;
        }
    }

    int cursor_x = 3;
    int cursor_y = TRAY_TOP + 3;
    int row_height = 0;

    for (size_t previous = 0; previous < existing_count; ++previous) {
        int next_x =
            s_die_views[previous].x +
            s_die_views[previous].width +
            SPAWN_GAP;

        if (next_x + view->width <= SCREEN_WIDTH - 3) {
            cursor_x = next_x;
            cursor_y = s_die_views[previous].y;
            row_height = s_die_views[previous].height;
        } else {
            cursor_x = 3;
            cursor_y =
                s_die_views[previous].y +
                s_die_views[previous].height +
                SPAWN_GAP;
            row_height = 0;
        }
    }

    if (cursor_y + view->height > TRAY_BOTTOM) {
        cursor_y = TRAY_TOP + (int)(existing_count % 5) * 8;
    }

    view->x = cursor_x;
    view->y = cursor_y;
    (void)row_height;
}

static void resolve_collisions(void)
{
    if (s_die_view_count > COLLISION_LIMIT) {
        return;
    }

    for (size_t first = 0; first < s_die_view_count; ++first) {
        for (size_t second = first + 1;
             second < s_die_view_count;
             ++second) {
            die_view_t *a = &s_die_views[first];
            die_view_t *b = &s_die_views[second];

            int overlap_x =
                (a->x < b->x)
                    ? (a->x + a->width - b->x)
                    : (b->x + b->width - a->x);
            int overlap_y =
                (a->y < b->y)
                    ? (a->y + a->height - b->y)
                    : (b->y + b->height - a->y);

            if (overlap_x <= 0 || overlap_y <= 0) {
                continue;
            }

            if (overlap_x < overlap_y) {
                int half = overlap_x / 2 + 1;

                if (a->x < b->x) {
                    a->x -= half;
                    b->x += half;
                } else {
                    a->x += half;
                    b->x -= half;
                }

                a->vx = -a->vx;
                b->vx = -b->vx;
            } else {
                int half = overlap_y / 2 + 1;

                if (a->y < b->y) {
                    a->y -= half;
                    b->y += half;
                } else {
                    a->y += half;
                    b->y -= half;
                }

                a->vy = -a->vy;
                b->vy = -b->vy;
            }
        }
    }
}

static void clamp_and_draw_die(die_view_t *view)
{
    if (view->x < 2) {
        view->x = 2;
        view->vx = abs(view->vx);
    } else if (view->x + view->width > SCREEN_WIDTH - 2) {
        view->x = SCREEN_WIDTH - view->width - 2;
        view->vx = -abs(view->vx);
    }

    if (view->y < TRAY_TOP) {
        view->y = TRAY_TOP;
        view->vy = abs(view->vy);
    } else if (view->y + view->height > TRAY_BOTTOM) {
        view->y = TRAY_BOTTOM - view->height;
        view->vy = -abs(view->vy);
    }

    lv_obj_set_pos(view->body, view->x, view->y);
}


static size_t result_detail_line_count(const char *details)
{
    if (details == NULL || details[0] == '\0') {
        return 0;
    }

    size_t lines = 1;
    for (const char *cursor = details; *cursor != '\0'; ++cursor) {
        if (*cursor == '\n') {
            ++lines;
        }
    }
    return lines;
}

static int custom_result_tray_top(void)
{
    size_t line_count =
        result_detail_line_count(s_custom_result_details);
    int tray_top = TRAY_TOP;

    if (line_count > RESULT_BASE_DETAIL_LINES) {
        tray_top +=
            (int)(line_count - RESULT_BASE_DETAIL_LINES) *
            RESULT_EXTRA_LINE_HEIGHT;
    }

    if (tray_top > RESULT_MAX_TRAY_TOP) {
        tray_top = RESULT_MAX_TRAY_TOP;
    }
    return tray_top;
}

static void arrange_custom_final_results(void)
{
    int x = 6;
    int y = custom_result_tray_top() + 10;
    int row_height = 0;

    for (size_t index = 0;
         index < s_die_view_count;
         ++index) {
        die_view_t *view = &s_die_views[index];
        const dice_custom_result_die_t *rolled =
            &s_custom_final_result.dice[view->result_index];

        if (x + view->width > SCREEN_WIDTH - 6) {
            x = 6;
            y += row_height + SPAWN_GAP;
            row_height = 0;
        }

        apply_custom_face(
            view,
            s_custom_set,
            rolled->die,
            rolled->face);

        lv_obj_set_pos(view->body, x, y);
        x += view->width + SPAWN_GAP;

        if (view->height > row_height) {
            row_height = view->height;
        }
    }

    char total_text[64];

    if (s_custom_result_title[0] != '\0') {
        lv_label_set_text(s_result_label, s_custom_result_title);
    } else {
        snprintf(
            total_text,
            sizeof(total_text),
            "Total: %ld",
            (long)s_custom_final_result.numeric_total);
        lv_label_set_text(s_result_label, total_text);
    }

    if (s_custom_result_details[0] != '\0') {
        lv_label_set_text(s_hint_label, s_custom_result_details);
    } else if (s_custom_final_result.total_count >
        s_custom_final_result.count) {
        char hint[64];

        snprintf(
            hint,
            sizeof(hint),
            "Showing %u of %u dice",
            (unsigned)s_custom_final_result.count,
            (unsigned)s_custom_final_result.total_count);

        lv_label_set_text(s_hint_label, hint);
    } else {
        lv_label_set_text(
            s_hint_label,
            "Shake to roll again");
    }

    if (s_custom_action_visible) {
        lv_obj_clear_flag(s_custom_action_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_custom_action_button);
    } else {
        lv_obj_add_flag(s_custom_action_button, LV_OBJ_FLAG_HIDDEN);
    }

    if (s_main_option_visible &&
        s_main_option_button != NULL) {
        lv_obj_clear_flag(
            s_main_option_button,
            LV_OBJ_FLAG_HIDDEN);
    }
}

static void arrange_final_results(void)
{
    int x = 6;
    int y = TRAY_TOP + 10;
    int row_height = 0;

    for (size_t index = 0; index < s_die_view_count; ++index) {
        die_view_t *view = &s_die_views[index];

        if (x + view->width > SCREEN_WIDTH - 6) {
            x = 6;
            y += row_height + SPAWN_GAP;
            row_height = 0;
        }

        set_die_face(view, &s_final_result.dice[index]);
        lv_obj_set_pos(view->body, x, y);
        x += view->width + SPAWN_GAP;

        if (view->height > row_height) {
            row_height = view->height;
        }
    }

    char total_text[40];
    snprintf(
        total_text,
        sizeof(total_text),
        "Total: %ld",
        (long)s_final_result.numeric_total);
    lv_label_set_text(s_result_label, total_text);

    if (s_final_result.total_count > s_final_result.count) {
        char hint[64];
        snprintf(
            hint,
            sizeof(hint),
            "Showing %u of %u dice",
            (unsigned)s_final_result.count,
            (unsigned)s_final_result.total_count);
        lv_label_set_text(s_hint_label, hint);
    } else {
        lv_label_set_text(s_hint_label, "Shake to roll again");
    }
}

static void animation_callback(lv_timer_t *timer)
{
    (void)timer;
    int64_t now_us = esp_timer_get_time();

    if (now_us - s_roll_started_us >= ROLL_DURATION_US) {
        lv_timer_delete(s_animation_timer);
        s_animation_timer = NULL;
        if (s_custom_mode) {
            arrange_custom_final_results();
        } else {
            arrange_final_results();
        }
        s_rolling = false;
        dice_power_high_performance_release();
        ESP_LOGI(TAG, "Animation complete");
        return;
    }

    for (size_t index = 0; index < s_die_view_count; ++index) {
        if (!s_die_views[index].animated) {
            continue;
        }

        s_die_views[index].x += s_die_views[index].vx;
        s_die_views[index].y += s_die_views[index].vy;
        clamp_and_draw_die(&s_die_views[index]);
    }

    resolve_collisions();

    for (size_t index = 0; index < s_die_view_count; ++index) {
        if (s_die_views[index].animated) {
            clamp_and_draw_die(&s_die_views[index]);
        }
    }

    if (now_us - s_last_face_change_us >= FACE_CHANGE_US) {
        s_last_face_change_us = now_us;

        for (size_t index = 0;
             index < s_die_view_count;
             ++index) {
            if (s_custom_mode) {
                if (!s_die_views[index].animated) {
                    continue;
                }

                const dice_custom_result_die_t *rolled =
                    &s_custom_final_result.dice[
                        s_die_views[index].result_index];
                const dice_set_face_t *preview =
                    dice_custom_random_face(
                        rolled->die,
                        NULL);

                if (preview != NULL) {
                    apply_custom_face(
                        &s_die_views[index],
                        s_custom_set,
                        rolled->die,
                        preview);
                }
            } else {
                die_result_t preview;
                dice_model_make_preview(
                    s_final_result.dice[index].definition,
                    &preview);
                set_die_face(
                    &s_die_views[index],
                    &preview);
            }
        }
    }
}


static void clear_message_hide_callback(lv_timer_t *timer)
{
    (void)timer;
    s_clear_message_timer = NULL;
    lv_obj_add_flag(s_clear_message, LV_OBJ_FLAG_HIDDEN);
}

static void show_notice_locked(const char *message)
{
    if (s_clear_message_timer != NULL) {
        lv_timer_delete(s_clear_message_timer);
        s_clear_message_timer = NULL;
    }

    lv_label_set_text(s_clear_message, message);
    lv_obj_clear_flag(s_clear_message, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_clear_message);
    lv_obj_move_foreground(s_input_layer);

    s_clear_message_timer = lv_timer_create(
        clear_message_hide_callback,
        CLEAR_MESSAGE_MS,
        NULL);
    lv_timer_set_repeat_count(s_clear_message_timer, 1);
}

static void about_close_callback(lv_event_t *event)
{
    (void)event;

    if (s_about_panel == NULL) {
        return;
    }

    lv_obj_add_flag(s_about_panel, LV_OBJ_FLAG_HIDDEN);
    if (s_menu_visible && s_menu != NULL) {
        lv_obj_clear_flag(s_menu, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_menu);
    }
}

static void menu_button_callback(lv_event_t *event)
{
    if (s_menu_scroll_dragging) {
        s_menu_scroll_dragging = false;
        return;
    }

    dice_ui_event_type_t type =
        (dice_ui_event_type_t)(intptr_t)lv_event_get_user_data(event);

    if (type == DICE_UI_EVENT_MENU_CLOSE) {
        emit_simple_event(type);
        return;
    }

    emit_simple_event(type);
}


static void set_button_callback(lv_event_t *event)
{
    size_t index =
        (size_t)(uintptr_t)lv_event_get_user_data(event);

    if (s_callback == NULL || index >= s_set_count) {
        return;
    }

    const dice_ui_event_t ui_event = {
        .type = DICE_UI_EVENT_SET_SELECTED,
        .selected_type = DICE_D4,
        .quantity_delta = 0,
        .set_index = index,
    };

    s_callback(&ui_event, s_callback_context);
}

static void close_rule_menu_from_ui(void)
{
    lv_obj_add_flag(
        s_rule_menu,
        LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(
        s_menu,
        LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(
        s_set_menu,
        LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(
        s_input_layer,
        LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(
        s_input_layer);
    s_menu_visible = false;
}

static void set_menu_swipe_callback(
    lv_event_t *event)
{
    lv_indev_t *input_device = lv_indev_active();
    if (input_device == NULL) {
        return;
    }

    lv_point_t point = {0};
    lv_indev_get_point(input_device, &point);
    lv_event_code_t code = lv_event_get_code(event);

    if (code == LV_EVENT_PRESSED) {
        s_set_menu_touching = true;
        s_set_menu_press_point = point;
        return;
    }

    if (code != LV_EVENT_RELEASED ||
        !s_set_menu_touching) {
        return;
    }

    s_set_menu_touching = false;

    int delta_x = point.x - s_set_menu_press_point.x;
    int delta_y = point.y - s_set_menu_press_point.y;

    if (delta_x <= -70 &&
        abs(delta_x) > abs(delta_y) + 20) {
        dice_ui_show_set_menu(false);
    }
}

static void rule_menu_swipe_callback(
    lv_event_t *event)
{
    lv_event_code_t code =
        lv_event_get_code(event);
    lv_indev_t *input_device =
        lv_indev_active();

    if (input_device == NULL) {
        return;
    }

    lv_point_t point = {0};
    lv_indev_get_point(
        input_device,
        &point);

    if (code == LV_EVENT_PRESSED) {
        s_rule_menu_touching = true;
        s_rule_menu_press_point = point;
        return;
    }

    if (code != LV_EVENT_RELEASED ||
        !s_rule_menu_touching) {
        return;
    }

    s_rule_menu_touching = false;

    int dx =
        point.x -
        s_rule_menu_press_point.x;
    int dy =
        point.y -
        s_rule_menu_press_point.y;

    if (dx <= -OPTIONS_SWIPE_THRESHOLD &&
        abs(dx) > abs(dy)) {
        close_rule_menu_from_ui();
    }
}


static void rule_clear_pool_callback(
    lv_event_t *event)
{
    (void)event;

    if (s_callback == NULL || s_rolling) {
        return;
    }

    close_rule_menu_from_ui();

    const dice_ui_event_t ui_event = {
        .type = DICE_UI_EVENT_CLEAR_POOL,
    };

    s_callback(
        &ui_event,
        s_callback_context);
}

static void rule_option_toggle_callback(
    lv_event_t *event)
{
    size_t index =
        (size_t)(uintptr_t)
            lv_event_get_user_data(event);

    if (s_callback == NULL ||
        index >= s_rule_option_count) {
        return;
    }

    const dice_ui_event_t ui_event = {
        .type =
            DICE_UI_EVENT_OPTION_CHANGED,
        .quantity_delta = 1,
        .option_index = index,
    };

    s_callback(
        &ui_event,
        s_callback_context);
}

static void rule_option_adjust_callback(
    lv_event_t *event)
{
    uintptr_t encoded =
        (uintptr_t)lv_event_get_user_data(
            event);

    size_t index = encoded >> 1;
    int direction =
        (encoded & 1u) != 0
            ? 1
            : -1;

    if (s_callback == NULL ||
        index >= s_rule_option_count) {
        return;
    }

    const dice_ui_event_t ui_event = {
        .type =
            DICE_UI_EVENT_OPTION_CHANGED,
        .quantity_delta = direction,
        .option_index = index,
    };

    s_callback(
        &ui_event,
        s_callback_context);
}

static void custom_action_callback(lv_event_t *event)
{
    (void)event;

    if (s_callback == NULL ||
        !s_custom_action_visible ||
        s_rolling) {
        return;
    }

    size_t action_index = s_custom_action_index;

    /*
     * Hide and disable immediately so a second tap cannot queue the same
     * one-use action while the controller task is processing the first.
     */
    s_custom_action_visible = false;
    lv_obj_add_flag(
        s_custom_action_button,
        LV_OBJ_FLAG_HIDDEN);

    close_rule_menu_from_ui();

    if (s_main_option_button != NULL) {
        lv_obj_add_flag(
            s_main_option_button,
            LV_OBJ_FLAG_HIDDEN);
    }

    const dice_ui_event_t ui_event = {
        .type = DICE_UI_EVENT_CUSTOM_ACTION,
        .action_index = action_index,
    };

    s_callback(&ui_event, s_callback_context);
}

static void style_unified_menu_button(lv_obj_t *button)
{
    lv_obj_set_style_bg_color(
        button, lv_color_hex(0x0d5f91), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(
        button, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(
        button, lv_color_hex(0x70d7ff), LV_PART_MAIN);
    lv_obj_set_style_border_width(
        button, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(
        button, 16, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(
        button, 0, LV_PART_MAIN);
}

static lv_obj_t *create_set_button(
    lv_obj_t *parent,
    size_t index)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_size(button, 290, RULE_MENU_ROW_HEIGHT);
    lv_obj_set_pos(
        button,
        4,
        (int)index * SET_MENU_ROW_HEIGHT);
    style_unified_menu_button(button);
    lv_obj_add_event_cb(
        button,
        set_button_callback,
        LV_EVENT_CLICKED,
        (void *)(uintptr_t)index);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_size(label, 250, 72);
    lv_obj_set_style_text_font(
        label, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(
        label, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_text_align(
        label,
        LV_TEXT_ALIGN_CENTER,
        LV_PART_MAIN);
    lv_obj_center(label);

    s_set_menu_buttons[index] = button;
    s_set_menu_labels[index] = label;
    return button;
}

static void menu_adjust_callback(lv_event_t *event)
{
    uintptr_t encoded =
        (uintptr_t)lv_event_get_user_data(event);
    dice_ui_event_type_t type =
        (dice_ui_event_type_t)(encoded >> 1);
    int direction =
        (encoded & 1u) != 0u ? 1 : -1;

    if (s_callback == NULL) {
        return;
    }

    const dice_ui_event_t ui_event = {
        .type = type,
        .quantity_delta = direction,
    };
    s_callback(&ui_event, s_callback_context);
}

static lv_obj_t *create_menu_adjust_button(
    lv_obj_t *parent,
    int y,
    const char *text,
    dice_ui_event_type_t type,
    lv_obj_t **label_out)
{
    lv_obj_t *button = lv_obj_create(parent);
    lv_obj_set_size(button, 290, 100);
    lv_obj_set_pos(button, 4, y);
    lv_obj_set_style_pad_all(button, 0, LV_PART_MAIN);
    style_unified_menu_button(button);
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(button, menu_scroll_callback, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(button, menu_scroll_callback, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(button, menu_scroll_callback, LV_EVENT_RELEASED, NULL);

    for (int side = 0; side < 2; ++side) {
        lv_obj_t *hit = lv_obj_create(button);
        lv_obj_set_size(hit, 78, 96);
        lv_obj_align(
            hit,
            side == 0 ? LV_ALIGN_LEFT_MID : LV_ALIGN_RIGHT_MID,
            0,
            0);
        lv_obj_set_style_bg_opa(hit, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(hit, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(hit, 0, LV_PART_MAIN);
        lv_obj_clear_flag(hit, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(hit, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
        uintptr_t encoded =
            ((uintptr_t)type << 1) | (side == 1 ? 1u : 0u);
        lv_obj_add_event_cb(
            hit,
            menu_adjust_callback,
            LV_EVENT_CLICKED,
            (void *)encoded);

        lv_obj_t *symbol = lv_label_create(button);
        lv_label_set_text(symbol, side == 0 ? "-" : "+");
        lv_obj_set_style_text_font(symbol, &lv_font_montserrat_32, LV_PART_MAIN);
        lv_obj_set_style_text_color(symbol, lv_color_hex(0xff73cf), LV_PART_MAIN);
        lv_obj_align(
            symbol,
            side == 0 ? LV_ALIGN_LEFT_MID : LV_ALIGN_RIGHT_MID,
            side == 0 ? 20 : -20,
            0);
    }

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_size(label, 190, 72);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_center(label);

    if (label_out != NULL) {
        *label_out = label;
    }
    return button;
}

static lv_obj_t *create_menu_button(
    lv_obj_t *parent,
    int y,
    const char *text,
    dice_ui_event_type_t type,
    lv_obj_t **label_out)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_size(button, 290, 100);
    lv_obj_set_pos(button, 4, y);
    style_unified_menu_button(button);
    lv_obj_add_event_cb(
        button,
        menu_scroll_callback,
        LV_EVENT_PRESSED,
        NULL);
    lv_obj_add_event_cb(
        button,
        menu_scroll_callback,
        LV_EVENT_PRESSING,
        NULL);
    lv_obj_add_event_cb(
        button,
        menu_scroll_callback,
        LV_EVENT_RELEASED,
        NULL);
    lv_obj_add_event_cb(
        button,
        menu_button_callback,
        LV_EVENT_CLICKED,
        (void *)(intptr_t)type);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_size(label, 266, 72);
    lv_obj_set_style_text_font(
        label, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(
        label, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_text_align(
        label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_center(label);

    if (label_out != NULL) {
        *label_out = label;
    }
    return button;
}

static int menu_maximum_offset(void)
{
    int maximum_offset =
        MENU_CONTENT_HEIGHT - MENU_VIEWPORT_HEIGHT;
    return maximum_offset > 0 ? maximum_offset : 0;
}

static int clamp_menu_scroll_offset(int value)
{
    int maximum_offset = menu_maximum_offset();
    if (value < 0) {
        return 0;
    }
    if (value > maximum_offset) {
        return maximum_offset;
    }
    return value;
}

static void set_menu_scroll_visual(int offset)
{
    const int maximum_offset = menu_maximum_offset();
    const int thumb_travel =
        MENU_SCROLL_TRACK_HEIGHT - MENU_SCROLL_THUMB_HEIGHT;
    const int clamped_offset =
        clamp_menu_scroll_offset(offset);

    lv_obj_set_y(s_menu_content, -offset);

    int thumb_y = 0;
    if (maximum_offset > 0) {
        thumb_y =
            clamped_offset * thumb_travel / maximum_offset;
    }
    lv_obj_set_y(s_menu_scroll_thumb, thumb_y);
}

static void update_menu_scroll_position(void)
{
    s_menu_scroll_offset =
        clamp_menu_scroll_offset(s_menu_scroll_offset);
    set_menu_scroll_visual(s_menu_scroll_offset);
}

static void menu_scroll_anim_exec(void *var, int32_t value)
{
    (void)var;
    s_menu_scroll_offset = (int)value;
    set_menu_scroll_visual(s_menu_scroll_offset);
}

static void menu_scroll_callback(lv_event_t *event)
{
    lv_indev_t *input_device = lv_indev_active();
    if (input_device == NULL) {
        return;
    }

    lv_point_t point = {0};
    lv_indev_get_point(input_device, &point);
    lv_event_code_t code = lv_event_get_code(event);

    if (code == LV_EVENT_PRESSED) {
        register_activity();
        dice_power_high_performance_acquire();
        s_menu_scroll_press_y = point.y;
        s_menu_scroll_start_offset = s_menu_scroll_offset;
        s_menu_scroll_dragging = false;
        return;
    }

    if (code == LV_EVENT_RELEASED) {
        int clamped = clamp_menu_scroll_offset(s_menu_scroll_offset);
        if (clamped != s_menu_scroll_offset) {
            lv_anim_t anim;
            lv_anim_init(&anim);
            lv_anim_set_var(&anim, NULL);
            lv_anim_set_exec_cb(&anim, menu_scroll_anim_exec);
            lv_anim_set_values(
                &anim, s_menu_scroll_offset, clamped);
            lv_anim_set_time(&anim, 160);
            lv_anim_start(&anim);
            s_menu_scroll_offset = clamped;
        }
        dice_power_high_performance_release();
        return;
    }

    if (code == LV_EVENT_PRESSING) {
        int delta_y = point.y - s_menu_scroll_press_y;
        const int maximum_offset = menu_maximum_offset();
        int next_offset =
            s_menu_scroll_start_offset - delta_y;

        if (abs(delta_y) >= 8) {
            s_menu_scroll_dragging = true;
        }

        if (next_offset < 0) {
            next_offset /= 3;
        } else if (next_offset > maximum_offset) {
            next_offset =
                maximum_offset +
                (next_offset - maximum_offset) / 3;
        }

        s_menu_scroll_offset = next_offset;
        set_menu_scroll_visual(s_menu_scroll_offset);
    }
}

static void style_overlay_panel(
    lv_obj_t *panel,
    lv_color_t background,
    lv_color_t border)
{
    lv_obj_set_style_bg_color(panel, background, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel, border, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(panel, 20, LV_PART_MAIN);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
}

static void build_screen(void)
{
    s_screen = active_screen();
    lv_obj_set_style_bg_color(
        s_screen, lv_color_hex(0x02050a), LV_PART_MAIN);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    s_result_label = lv_label_create(s_screen);
    lv_label_set_text(s_result_label, "Ready");
    lv_obj_set_style_text_font(
        s_result_label, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(
        s_result_label, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_align(s_result_label, LV_ALIGN_TOP_MID, 0, 12);

    s_hint_label = lv_label_create(s_screen);
    lv_label_set_text(s_hint_label, "Touch to choose dice, then shake");
    lv_obj_set_width(s_hint_label, SCREEN_WIDTH - 24);
    lv_obj_set_style_text_align(
        s_hint_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(s_hint_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(
        s_hint_label, lv_color_hex(0xb9dcff), LV_PART_MAIN);
    lv_obj_align(s_hint_label, LV_ALIGN_TOP_MID, 0, 44);

    s_main_option_button = lv_button_create(s_screen);
    lv_obj_set_size(
        s_main_option_button,
        MAIN_OPTION_WIDTH,
        MAIN_OPTION_HEIGHT);
    lv_obj_set_pos(
        s_main_option_button,
        MAIN_OPTION_X,
        MAIN_OPTION_Y);
    lv_obj_set_style_bg_color(
        s_main_option_button,
        lv_color_hex(0x173653),
        LV_PART_MAIN);
    lv_obj_set_style_border_color(
        s_main_option_button,
        lv_color_hex(0x52bfff),
        LV_PART_MAIN);
    lv_obj_set_style_border_width(
        s_main_option_button,
        1,
        LV_PART_MAIN);
    lv_obj_set_style_radius(
        s_main_option_button,
        15,
        LV_PART_MAIN);
    lv_obj_set_style_pad_all(
        s_main_option_button,
        4,
        LV_PART_MAIN);
    lv_obj_clear_flag(
        s_main_option_button,
        LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    s_main_option_label =
        lv_label_create(s_main_option_button);
    lv_obj_set_style_text_font(
        s_main_option_label,
        &lv_font_montserrat_14,
        LV_PART_MAIN);
    lv_obj_set_style_text_color(
        s_main_option_label,
        lv_color_hex(0xffffff),
        LV_PART_MAIN);
    lv_label_set_text(
        s_main_option_label,
        "Success on: 5+");
    lv_obj_center(s_main_option_label);
    lv_obj_add_flag(
        s_main_option_button,
        LV_OBJ_FLAG_HIDDEN);

    s_pool_label = lv_label_create(s_screen);
    lv_obj_set_width(s_pool_label, SCREEN_WIDTH - 20);
    lv_obj_set_style_text_align(
        s_pool_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(
        s_pool_label, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_align(s_pool_label, LV_ALIGN_BOTTOM_MID, 0, -15);

    lv_obj_t *hamburger_line1 = lv_obj_create(s_screen);
    lv_obj_set_size(hamburger_line1, 18, 3);
    lv_obj_set_pos(hamburger_line1, 14, 18);
    lv_obj_set_style_bg_color(hamburger_line1, lv_color_hex(0x6a7480), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(hamburger_line1, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_border_width(hamburger_line1, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(hamburger_line1, 2, LV_PART_MAIN);
    lv_obj_clear_flag(hamburger_line1, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *hamburger_line2 = lv_obj_create(s_screen);
    lv_obj_set_size(hamburger_line2, 18, 3);
    lv_obj_set_pos(hamburger_line2, 14, 25);
    lv_obj_set_style_bg_color(hamburger_line2, lv_color_hex(0x6a7480), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(hamburger_line2, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_border_width(hamburger_line2, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(hamburger_line2, 2, LV_PART_MAIN);
    lv_obj_clear_flag(hamburger_line2, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *hamburger_line3 = lv_obj_create(s_screen);
    lv_obj_set_size(hamburger_line3, 18, 3);
    lv_obj_set_pos(hamburger_line3, 14, 32);
    lv_obj_set_style_bg_color(hamburger_line3, lv_color_hex(0x6a7480), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(hamburger_line3, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_border_width(hamburger_line3, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(hamburger_line3, 2, LV_PART_MAIN);
    lv_obj_clear_flag(hamburger_line3, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    s_selector = lv_obj_create(s_screen);
    lv_obj_set_size(s_selector, 334, 282);
    lv_obj_align(s_selector, LV_ALIGN_CENTER, -14, -38);
    style_overlay_panel(
        s_selector,
        lv_color_hex(0x07131f),
        lv_color_hex(0x39b8ff));
    lv_obj_add_flag(s_selector, LV_OBJ_FLAG_HIDDEN);

    s_type_selection_box = lv_obj_create(s_selector);
    lv_obj_set_size(s_type_selection_box, 156, 72);
    lv_obj_align(
        s_type_selection_box,
        LV_ALIGN_TOP_MID,
        0,
        30);
    lv_obj_set_style_bg_opa(
        s_type_selection_box, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_color(
        s_type_selection_box, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_border_width(
        s_type_selection_box, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(
        s_type_selection_box, 8, LV_PART_MAIN);
    lv_obj_clear_flag(
        s_type_selection_box,
        LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    s_quantity_wheel_bar = lv_obj_create(s_selector);
    lv_obj_set_size(s_quantity_wheel_bar, 132, 156);
    lv_obj_align(
        s_quantity_wheel_bar,
        LV_ALIGN_TOP_MID,
        0,
        128);
    lv_obj_set_style_bg_color(
        s_quantity_wheel_bar,
        lv_color_hex(0x111923),
        LV_PART_MAIN);
    lv_obj_set_style_bg_opa(
        s_quantity_wheel_bar,
        LV_OPA_70,
        LV_PART_MAIN);
    lv_obj_set_style_border_width(
        s_quantity_wheel_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(
        s_quantity_wheel_bar, 12, LV_PART_MAIN);
    lv_obj_clear_flag(
        s_quantity_wheel_bar,
        LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(
        s_quantity_wheel_bar,
        LV_OBJ_FLAG_HIDDEN);

    s_quantity_selection_box = lv_obj_create(s_selector);
    lv_obj_set_size(s_quantity_selection_box, 96, 56);
    lv_obj_align(
        s_quantity_selection_box,
        LV_ALIGN_TOP_MID,
        0,
        178);
    lv_obj_set_style_bg_color(
        s_quantity_selection_box,
        lv_color_hex(0x17150b),
        LV_PART_MAIN);
    lv_obj_set_style_bg_opa(
        s_quantity_selection_box,
        LV_OPA_70,
        LV_PART_MAIN);
    lv_obj_set_style_border_color(
        s_quantity_selection_box,
        lv_color_hex(0xffdc68),
        LV_PART_MAIN);
    lv_obj_set_style_border_width(
        s_quantity_selection_box, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(
        s_quantity_selection_box, 8, LV_PART_MAIN);
    lv_obj_clear_flag(
        s_quantity_selection_box,
        LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    for (int index = 0; index < WHEEL_ITEM_COUNT; ++index) {
        s_type_labels[index] = lv_label_create(s_selector);
        lv_obj_set_style_text_font(
            s_type_labels[index],
            &lv_font_montserrat_18,
            LV_PART_MAIN);
        lv_obj_set_style_text_align(
            s_type_labels[index],
            LV_TEXT_ALIGN_CENTER,
            LV_PART_MAIN);
        lv_label_set_long_mode(
            s_type_labels[index],
            LV_LABEL_LONG_CLIP);

        s_quantity_labels[index] = lv_label_create(s_selector);
        lv_obj_set_style_text_font(
            s_quantity_labels[index],
            &lv_font_montserrat_24,
            LV_PART_MAIN);
        lv_obj_set_style_text_align(
            s_quantity_labels[index],
            LV_TEXT_ALIGN_CENTER,
            LV_PART_MAIN);
    }

    s_selector_quantity_note = lv_label_create(s_selector);
    lv_obj_set_width(s_selector_quantity_note, 300);
    lv_obj_set_style_text_align(
        s_selector_quantity_note,
        LV_TEXT_ALIGN_CENTER,
        LV_PART_MAIN);
    lv_obj_set_style_text_color(
        s_selector_quantity_note,
        lv_color_hex(0xff9de5),
        LV_PART_MAIN);
    lv_obj_align(
        s_selector_quantity_note,
        LV_ALIGN_BOTTOM_MID,
        0,
        -12);

    position_selector_wheels();

    s_menu = lv_obj_create(s_screen);
    lv_obj_set_size(s_menu, 344, 420);
    lv_obj_align(s_menu, LV_ALIGN_CENTER, 0, 0);
    style_overlay_panel(
        s_menu,
        lv_color_hex(0x050b11),
        lv_color_hex(0xffd45c));
    lv_obj_set_style_pad_all(s_menu, 6, LV_PART_MAIN);
    lv_obj_clear_flag(s_menu, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_menu, LV_OBJ_FLAG_HIDDEN);

    s_menu_battery_label = lv_label_create(s_menu);
    lv_label_set_text(
        s_menu_battery_label,
        "BATTERY  --.-- V  --%");
    lv_obj_set_width(s_menu_battery_label, 260);
    lv_obj_set_style_text_font(
        s_menu_battery_label,
        &lv_font_montserrat_18,
        LV_PART_MAIN);
    lv_obj_set_style_text_align(
        s_menu_battery_label,
        LV_TEXT_ALIGN_CENTER,
        LV_PART_MAIN);
    lv_obj_set_style_text_color(
        s_menu_battery_label,
        lv_color_hex(0xffdc68),
        LV_PART_MAIN);
    lv_obj_set_pos(s_menu_battery_label, 4, 10);

    s_menu_viewport = lv_obj_create(s_menu);
    lv_obj_set_size(s_menu_viewport, 306, MENU_VIEWPORT_HEIGHT);
    lv_obj_set_pos(s_menu_viewport, 4, 48);
    lv_obj_add_event_cb(
        s_menu_viewport,
        menu_scroll_callback,
        LV_EVENT_PRESSED,
        NULL);
    lv_obj_add_event_cb(
        s_menu_viewport,
        menu_scroll_callback,
        LV_EVENT_PRESSING,
        NULL);
    lv_obj_add_event_cb(
        s_menu_viewport,
        menu_scroll_callback,
        LV_EVENT_RELEASED,
        NULL);
    lv_obj_set_style_bg_opa(
        s_menu_viewport, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(
        s_menu_viewport, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(
        s_menu_viewport, 0, LV_PART_MAIN);
    lv_obj_clear_flag(
        s_menu_viewport, LV_OBJ_FLAG_SCROLLABLE);

    s_menu_content = lv_obj_create(s_menu_viewport);
    lv_obj_set_size(
        s_menu_content, 304, MENU_CONTENT_HEIGHT);
    lv_obj_set_pos(s_menu_content, 0, 0);
    lv_obj_set_style_bg_opa(
        s_menu_content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(
        s_menu_content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(
        s_menu_content, 0, LV_PART_MAIN);
    lv_obj_clear_flag(
        s_menu_content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(
        s_menu_content,
        menu_scroll_callback,
        LV_EVENT_PRESSED,
        NULL);
    lv_obj_add_event_cb(
        s_menu_content,
        menu_scroll_callback,
        LV_EVENT_PRESSING,
        NULL);
    lv_obj_add_event_cb(
        s_menu_content,
        menu_scroll_callback,
        LV_EVENT_RELEASED,
        NULL);

    create_menu_button(
        s_menu_content, 0, "Set: Standard",
        DICE_UI_EVENT_MENU_SET,
        &s_menu_set_label);
    create_menu_button(
        s_menu_content, 112, "Set options",
        DICE_UI_EVENT_MENU_SET_OPTIONS, NULL);
    create_menu_button(
        s_menu_content, 224, "Sound: On",
        DICE_UI_EVENT_MENU_SOUND,
        &s_menu_sound_label);
    create_menu_button(
        s_menu_content, 336, "Brightness: 65%",
        DICE_UI_EVENT_MENU_BRIGHTNESS,
        &s_menu_brightness_label);
    create_menu_button(
        s_menu_content, 448, "Shake sensitivity: Normal",
        DICE_UI_EVENT_MENU_SHAKE,
        &s_menu_shake_label);
    create_menu_adjust_button(
        s_menu_content, 560, "Display timeout\n60 sec",
        DICE_UI_EVENT_MENU_TIMEOUT,
        &s_menu_timeout_label);
    create_menu_adjust_button(
        s_menu_content, 672, "Selector return\n1.0 sec",
        DICE_UI_EVENT_MENU_SELECTOR_DELAY,
        &s_menu_selector_delay_label);
    create_menu_button(
        s_menu_content, 784, "USB transfer mode",
        DICE_UI_EVENT_MENU_TRANSFER,
        NULL);
    create_menu_button(
        s_menu_content, 896, "About / status",
        DICE_UI_EVENT_MENU_ABOUT,
        NULL);

    s_menu_scroll_hit_area = lv_obj_create(s_menu);
    lv_obj_set_size(
        s_menu_scroll_hit_area, 1, 1);
    lv_obj_set_pos(s_menu_scroll_hit_area, 0, 0);
    lv_obj_set_style_bg_opa(
        s_menu_scroll_hit_area, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(
        s_menu_scroll_hit_area, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(
        s_menu_scroll_hit_area, 0, LV_PART_MAIN);
    lv_obj_clear_flag(
        s_menu_scroll_hit_area, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(
        s_menu_scroll_hit_area, LV_OBJ_FLAG_HIDDEN);

    s_menu_scroll_track = lv_obj_create(s_menu);
    lv_obj_set_size(
        s_menu_scroll_track, 8, MENU_SCROLL_TRACK_HEIGHT);
    lv_obj_set_pos(s_menu_scroll_track, 322, 48);
    lv_obj_set_style_bg_opa(
        s_menu_scroll_track, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(
        s_menu_scroll_track, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(
        s_menu_scroll_track, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_all(
        s_menu_scroll_track, 0, LV_PART_MAIN);
    lv_obj_clear_flag(
        s_menu_scroll_track, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(
        s_menu_scroll_track, LV_OBJ_FLAG_CLICKABLE);

    s_menu_scroll_thumb = lv_obj_create(s_menu_scroll_track);
    lv_obj_set_size(
        s_menu_scroll_thumb, 6, MENU_SCROLL_THUMB_HEIGHT);
    lv_obj_align(
        s_menu_scroll_thumb, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(
        s_menu_scroll_thumb,
        lv_color_hex(0xf0f0f0),
        LV_PART_MAIN);
    lv_obj_set_style_bg_opa(
        s_menu_scroll_thumb, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(
        s_menu_scroll_thumb, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(
        s_menu_scroll_thumb, 4, LV_PART_MAIN);
    lv_obj_clear_flag(
        s_menu_scroll_thumb,
        LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    lv_obj_move_foreground(s_menu_scroll_track);
    lv_obj_move_foreground(s_menu_scroll_thumb);

    s_menu_scroll_offset = 0;
    update_menu_scroll_position();

    s_set_menu = lv_obj_create(s_screen);
    lv_obj_set_size(s_set_menu, 344, 420);
    lv_obj_align(s_set_menu, LV_ALIGN_CENTER, 0, 0);
    style_overlay_panel(
        s_set_menu,
        lv_color_hex(0x050b11),
        lv_color_hex(0xd5b6ff));
    lv_obj_set_style_pad_all(s_set_menu, 8, LV_PART_MAIN);
    lv_obj_clear_flag(s_set_menu, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_set_menu, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(
        s_set_menu,
        set_menu_swipe_callback,
        LV_EVENT_PRESSED,
        NULL);
    lv_obj_add_event_cb(
        s_set_menu,
        set_menu_swipe_callback,
        LV_EVENT_RELEASED,
        NULL);

    s_set_menu_title = lv_label_create(s_set_menu);
    lv_label_set_text(s_set_menu_title, "Select dice set");
    lv_obj_set_style_text_font(
        s_set_menu_title,
        &lv_font_montserrat_24,
        LV_PART_MAIN);
    lv_obj_set_style_text_color(
        s_set_menu_title,
        lv_color_hex(0xffffff),
        LV_PART_MAIN);
    lv_obj_align(s_set_menu_title, LV_ALIGN_TOP_MID, 0, 4);

    s_set_menu_list = lv_obj_create(s_set_menu);
    lv_obj_set_size(s_set_menu_list, 310, 350);
    lv_obj_align(s_set_menu_list, LV_ALIGN_TOP_LEFT, 4, 46);
    lv_obj_set_style_pad_all(
        s_set_menu_list,
        4,
        LV_PART_MAIN);
    lv_obj_set_style_bg_color(
        s_set_menu_list,
        lv_color_hex(0x09131d),
        LV_PART_MAIN);
    lv_obj_set_style_border_width(
        s_set_menu_list,
        0,
        LV_PART_MAIN);
    lv_obj_set_style_radius(
        s_set_menu_list,
        0,
        LV_PART_MAIN);
    lv_obj_set_style_width(
        s_set_menu_list,
        8,
        LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(
        s_set_menu_list,
        lv_color_hex(0xf0f0f0),
        LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(
        s_set_menu_list,
        LV_OPA_COVER,
        LV_PART_SCROLLBAR);
    lv_obj_set_style_border_width(
        s_set_menu_list,
        0,
        LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(
        s_set_menu_list,
        4,
        LV_PART_SCROLLBAR);
    lv_obj_set_scroll_dir(
        s_set_menu_list,
        LV_DIR_VER);
    lv_obj_set_scrollbar_mode(
        s_set_menu_list,
        LV_SCROLLBAR_MODE_AUTO);
    lv_obj_add_event_cb(
        s_set_menu_list,
        set_menu_swipe_callback,
        LV_EVENT_PRESSED,
        NULL);
    lv_obj_add_event_cb(
        s_set_menu_list,
        set_menu_swipe_callback,
        LV_EVENT_RELEASED,
        NULL);

    s_rule_menu = lv_obj_create(s_screen);
    lv_obj_set_size(s_rule_menu, 344, 420);
    lv_obj_align(s_rule_menu, LV_ALIGN_CENTER, 0, 0);
    style_overlay_panel(
        s_rule_menu,
        lv_color_hex(0x050b11),
        lv_color_hex(0x9fe870));
    lv_obj_set_style_pad_all(s_rule_menu, 8, LV_PART_MAIN);
    lv_obj_clear_flag(s_rule_menu, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_rule_menu, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(
        s_rule_menu,
        rule_menu_swipe_callback,
        LV_EVENT_PRESSED,
        NULL);
    lv_obj_add_event_cb(
        s_rule_menu,
        rule_menu_swipe_callback,
        LV_EVENT_RELEASED,
        NULL);

    lv_obj_t *rule_title = lv_label_create(s_rule_menu);
    lv_label_set_text(rule_title, "Roll Options - swipe left to exit");
    lv_obj_set_style_text_font(rule_title, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(rule_title, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_align(rule_title, LV_ALIGN_TOP_MID, 0, 4);

    s_rule_menu_list = lv_obj_create(s_rule_menu);
    lv_obj_set_size(s_rule_menu_list, 310, 350);
    lv_obj_align(s_rule_menu_list, LV_ALIGN_TOP_LEFT, 4, 46);
    lv_obj_set_style_bg_color(
        s_rule_menu_list,
        lv_color_hex(0x050b11),
        LV_PART_MAIN);
    lv_obj_set_style_width(
        s_rule_menu_list,
        310,
        LV_PART_MAIN);
    lv_obj_set_style_width(
        s_rule_menu_list,
        8,
        LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(
        s_rule_menu_list,
        lv_color_hex(0xf0f0f0),
        LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(
        s_rule_menu_list,
        LV_OPA_COVER,
        LV_PART_SCROLLBAR);
    lv_obj_set_style_border_width(
        s_rule_menu_list,
        0,
        LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(
        s_rule_menu_list,
        4,
        LV_PART_SCROLLBAR);
    lv_obj_set_style_border_width(
        s_rule_menu_list,
        0,
        LV_PART_MAIN);
    lv_obj_set_scroll_dir(s_rule_menu_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_rule_menu_list, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_add_event_cb(
        s_rule_menu_list,
        rule_menu_swipe_callback,
        LV_EVENT_PRESSED,
        NULL);
    lv_obj_add_event_cb(
        s_rule_menu_list,
        rule_menu_swipe_callback,
        LV_EVENT_RELEASED,
        NULL);

    s_rule_clear_button =
        lv_button_create(s_rule_menu_list);
    lv_obj_set_size(
        s_rule_clear_button,
        RULE_MENU_ROW_WIDTH,
        RULE_MENU_ROW_HEIGHT);
    lv_obj_set_pos(
        s_rule_clear_button,
        4,
        0);
    style_unified_menu_button(s_rule_clear_button);
    lv_obj_add_event_cb(
        s_rule_clear_button,
        rule_clear_pool_callback,
        LV_EVENT_CLICKED,
        NULL);
    lv_obj_add_flag(
        s_rule_clear_button,
        LV_OBJ_FLAG_GESTURE_BUBBLE);

    lv_obj_t *clear_label =
        lv_label_create(s_rule_clear_button);
    lv_label_set_text(
        clear_label,
        "Clear dice pool");
    lv_obj_set_style_text_font(
        clear_label,
        &lv_font_montserrat_24,
        LV_PART_MAIN);
    lv_obj_set_style_text_color(
        clear_label,
        lv_color_hex(0xffffff),
        LV_PART_MAIN);
    lv_obj_set_style_text_align(
        clear_label,
        LV_TEXT_ALIGN_CENTER,
        LV_PART_MAIN);
    lv_obj_set_size(clear_label, 240, 64);
    lv_obj_center(clear_label);

    s_custom_action_button = lv_button_create(s_rule_menu_list);
    lv_obj_set_size(
        s_custom_action_button,
        RULE_MENU_ROW_WIDTH,
        RULE_MENU_ROW_HEIGHT);
    lv_obj_set_pos(
        s_custom_action_button,
        4,
        RULE_OPTION_ROW_HEIGHT);
    style_unified_menu_button(s_custom_action_button);
    lv_obj_add_event_cb(
        s_custom_action_button,
        custom_action_callback,
        LV_EVENT_CLICKED,
        NULL);
    s_custom_action_label = lv_label_create(s_custom_action_button);
    lv_label_set_text(s_custom_action_label, "Reroll failures");
    lv_obj_set_style_text_font(
        s_custom_action_label, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(
        s_custom_action_label, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_text_align(
        s_custom_action_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_size(s_custom_action_label, 240, 64);
    lv_obj_center(s_custom_action_label);
    lv_obj_add_flag(s_custom_action_button, LV_OBJ_FLAG_HIDDEN);

    s_clear_message = lv_label_create(s_screen);
    lv_obj_set_style_bg_color(
        s_clear_message, lv_color_hex(0x0d2130), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(
        s_clear_message, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(
        s_clear_message, lv_color_hex(0x5bd6ff), LV_PART_MAIN);
    lv_obj_set_style_border_width(
        s_clear_message, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(
        s_clear_message, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_all(
        s_clear_message, 10, LV_PART_MAIN);
    lv_obj_set_style_text_color(
        s_clear_message, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_label_set_text(s_clear_message, "Dice pool cleared");
    lv_obj_align(s_clear_message, LV_ALIGN_CENTER, 0, -120);
    lv_obj_add_flag(s_clear_message, LV_OBJ_FLAG_HIDDEN);

    s_input_layer = lv_obj_create(s_screen);
    lv_obj_set_size(s_input_layer, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_pos(s_input_layer, 0, 0);
    lv_obj_set_style_bg_opa(
        s_input_layer, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(
        s_input_layer, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(
        s_input_layer, 0, LV_PART_MAIN);
    lv_obj_clear_flag(s_input_layer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_input_layer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(
        s_input_layer, touch_callback, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(
        s_input_layer, touch_callback, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(
        s_input_layer, touch_callback, LV_EVENT_RELEASED, NULL);

    s_about_panel = lv_obj_create(s_screen);
    lv_obj_set_size(s_about_panel, 344, 420);
    lv_obj_align(s_about_panel, LV_ALIGN_CENTER, 0, 0);
    style_overlay_panel(
        s_about_panel,
        lv_color_hex(0x050b11),
        lv_color_hex(0x5bd6ff));
    lv_obj_set_style_pad_all(s_about_panel, 8, LV_PART_MAIN);
    lv_obj_clear_flag(s_about_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_about_panel, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *about_title = lv_label_create(s_about_panel);
    lv_label_set_text(about_title, "About / status");
    lv_obj_set_style_text_font(
        about_title, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(
        about_title, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_align(about_title, LV_ALIGN_TOP_MID, 0, 2);

    lv_obj_t *about_scroller = lv_obj_create(s_about_panel);
    lv_obj_set_size(about_scroller, 310, 316);
    lv_obj_align(about_scroller, LV_ALIGN_TOP_MID, 0, 44);
    lv_obj_set_style_bg_color(
        about_scroller, lv_color_hex(0x09131d), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(
        about_scroller, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(about_scroller, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(about_scroller, 8, LV_PART_MAIN);
    lv_obj_set_scroll_dir(about_scroller, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(
        about_scroller, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_width(about_scroller, 7, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(
        about_scroller, lv_color_hex(0xf0f0f0), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(
        about_scroller, LV_OPA_COVER, LV_PART_SCROLLBAR);
    lv_obj_set_style_border_width(
        about_scroller, 0, LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(about_scroller, 4, LV_PART_SCROLLBAR);

    s_about_content_label = lv_label_create(about_scroller);
    lv_obj_set_width(s_about_content_label, 282);
    lv_label_set_long_mode(
        s_about_content_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(
        s_about_content_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(
        s_about_content_label, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_text_line_space(
        s_about_content_label, 4, LV_PART_MAIN);
    lv_label_set_text(s_about_content_label, "Spiffy Roller v1.0");
    lv_obj_align(s_about_content_label, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *about_close = lv_button_create(s_about_panel);
    lv_obj_set_size(about_close, 150, 44);
    lv_obj_align(about_close, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_set_style_bg_color(
        about_close, lv_color_hex(0x0d4c70), LV_PART_MAIN);
    lv_obj_set_style_border_color(
        about_close, lv_color_hex(0x70d7ff), LV_PART_MAIN);
    lv_obj_set_style_border_width(about_close, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(about_close, 8, LV_PART_MAIN);
    lv_obj_add_event_cb(
        about_close, about_close_callback, LV_EVENT_CLICKED, NULL);

    lv_obj_t *about_close_label = lv_label_create(about_close);
    lv_label_set_text(about_close_label, "Back");
    lv_obj_set_style_text_font(
        about_close_label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(
        about_close_label, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_center(about_close_label);
}

bool dice_ui_startup_begin(const char *version)
{
    if (s_display == NULL) {
        s_display = bsp_display_start();
    }
    if (s_display == NULL) {
        return false;
    }

    if (bsp_display_brightness_set(65) != ESP_OK ||
        !bsp_display_lock(2000)) {
        return false;
    }

    lv_obj_t *screen = lv_screen_active();
    lv_obj_clean(screen);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x080B12), 0);

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "Spiffy Roller");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -66);

    lv_obj_t *version_label = lv_label_create(screen);
    lv_label_set_text(version_label, version != NULL ? version : "");
    lv_obj_set_style_text_color(
        version_label, lv_color_hex(0xAEB8C8), 0);
    lv_obj_set_style_text_font(
        version_label, &lv_font_montserrat_14, 0);
    lv_obj_align(version_label, LV_ALIGN_CENTER, 0, -25);

    lv_obj_t *spinner = lv_spinner_create(screen);
    lv_obj_set_size(spinner, 42, 42);
    lv_obj_set_style_arc_width(spinner, 5, LV_PART_MAIN);
    lv_obj_set_style_arc_width(spinner, 5, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(
        spinner, lv_color_hex(0x30394A), LV_PART_MAIN);
    lv_obj_set_style_arc_color(
        spinner, lv_color_hex(0xC6A8FF), LV_PART_INDICATOR);
    lv_obj_align(spinner, LV_ALIGN_CENTER, 0, 31);

    s_startup_status_label = lv_label_create(screen);
    lv_obj_set_width(s_startup_status_label, SCREEN_WIDTH - 36);
    lv_obj_set_style_text_align(
        s_startup_status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(
        s_startup_status_label, lv_color_hex(0xAEB8C8), 0);
    lv_obj_set_style_text_font(
        s_startup_status_label, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(s_startup_status_label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(s_startup_status_label, "Starting...");
    lv_obj_align(s_startup_status_label, LV_ALIGN_BOTTOM_MID, 0, -54);

    lv_obj_invalidate(screen);
    lv_refr_now(NULL);
    bsp_display_unlock();
    return true;
}

void dice_ui_startup_status(const char *message)
{
    if (s_startup_status_label == NULL ||
        !bsp_display_lock(500)) {
        return;
    }

    lv_label_set_text(
        s_startup_status_label,
        message != NULL ? message : "Starting...");
    lv_obj_invalidate(s_startup_status_label);
    lv_refr_now(NULL);
    bsp_display_unlock();
}

void dice_ui_startup_preload_set(const dice_set_definition_t *set)
{
    dice_face_assets_release_all();
    dice_face_assets_preload_set(set);
    s_startup_preloaded_set = set;
}

bool dice_ui_start(
    dice_ui_event_callback_t callback,
    void *context,
    const dice_pool_t *initial_pool)
{
    s_callback = callback;
    s_callback_context = context;

    if (initial_pool != NULL) {
        s_pool = *initial_pool;
    }

    if (s_display == NULL && !dice_ui_startup_begin("v1.0")) {
        return false;
    }

    if (!bsp_display_lock(2000)) {
        return false;
    }

    lv_obj_clean(lv_screen_active());
    s_startup_status_label = NULL;
    build_screen();
    update_pool_text();
    s_last_activity_us = esp_timer_get_time();
    s_inactivity_timer = lv_timer_create(
        inactivity_timer_callback,
        1000,
        NULL);

    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(s_display);

    bsp_display_unlock();
    return true;
}

void dice_ui_wake_display(void)
{
    if (!bsp_display_lock(250)) {
        return;
    }

    register_activity();
    bsp_display_unlock();
}

bool dice_ui_is_display_dimmed(void)
{
    return s_screen_dimmed;
}

void dice_ui_set_pool(const dice_pool_t *new_pool)
{
    if (new_pool == NULL || !bsp_display_lock(250)) {
        return;
    }

    s_pool = *new_pool;
    update_pool_text();
    update_selector_text();
    bsp_display_unlock();
}

void dice_ui_start_roll_animation(const dice_roll_result_t *result)
{
    s_custom_mode = false;

    if (result == NULL || result->count == 0 ||
        !bsp_display_lock(500)) {
        return;
    }

    register_activity();
    cancel_selector_hide();
    lv_obj_add_flag(s_selector, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_menu, LV_OBJ_FLAG_HIDDEN);
    s_menu_visible = false;
    clear_die_views();
    refresh_after_hiding_overlay();

    s_final_result = *result;
    dice_model_sort_result(&s_final_result);
    s_die_view_count = s_final_result.count;
    dice_power_high_performance_acquire();
    s_rolling = true;
    s_roll_started_us = esp_timer_get_time();
    s_last_face_change_us = s_roll_started_us;

    lv_label_set_text(s_result_label, "Rolling...");
    lv_label_set_text(s_hint_label, "");

    size_t physical_count = s_die_view_count;
    for (size_t index = 0; index < s_die_view_count; ++index) {
        if (s_final_result.dice[index].kind ==
            DIE_RESULT_PERCENTILE) {
            ++physical_count;
        }
    }

    int unit_size = animation_unit_size(physical_count);

    for (size_t index = 0; index < s_die_view_count; ++index) {
        die_view_t *view = &s_die_views[index];
        const die_result_t *face = &s_final_result.dice[index];

        create_die_graphics(
            view,
            unit_size,
            face->definition->type);
        view->animated = true;
        choose_spawn_position(view, index);

        view->vx = (int)(esp_random() % 9) - 4;
        view->vy = (int)(esp_random() % 9) - 4;

        if (view->vx == 0) {
            view->vx = 3;
        }
        if (view->vy == 0) {
            view->vy = -3;
        }

        lv_obj_set_pos(view->body, view->x, view->y);

        die_result_t preview;
        dice_model_make_preview(face->definition, &preview);
        set_die_face(view, &preview);
    }

    lv_obj_move_foreground(s_input_layer);
    s_animation_timer = lv_timer_create(
        animation_callback,
        ANIMATION_TIMER_MS,
        NULL);
    bsp_display_unlock();
}


void dice_ui_start_custom_roll_animation(
    const dice_set_definition_t *set,
    const dice_custom_roll_result_t *result)
{
    if (set == NULL ||
        result == NULL ||
        result->count == 0 ||
        !bsp_display_lock(500)) {
        return;
    }

    s_custom_mode = true;
    s_custom_set = set;

    register_activity();
    cancel_selector_hide();
    lv_obj_add_flag(s_selector, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_menu, LV_OBJ_FLAG_HIDDEN);
    s_menu_visible = false;
    clear_die_views();
    refresh_after_hiding_overlay();

    s_custom_final_result = *result;
    s_die_view_count = result->count;
    lv_obj_add_flag(s_custom_action_button, LV_OBJ_FLAG_HIDDEN);
    dice_power_high_performance_acquire();
    s_rolling = true;
    s_roll_started_us = esp_timer_get_time();
    s_last_face_change_us = s_roll_started_us;

    lv_label_set_text(s_result_label, "Rolling...");
    lv_label_set_text(s_hint_label, "");

    int unit_size =
        animation_unit_size(s_die_view_count);

    for (size_t index = 0;
         index < s_die_view_count;
         ++index) {
        die_view_t *view = &s_die_views[index];
        const dice_custom_result_die_t *rolled =
            &s_custom_final_result.dice[index];

        view->animated = true;
        view->result_index = index;

        create_custom_die_graphics(
            view,
            unit_size,
            rolled->die_index,
            rolled->die);

        choose_spawn_position(view, index);

        view->vx = (int)(esp_random() % 9) - 4;
        view->vy = (int)(esp_random() % 9) - 4;

        if (view->vx == 0) {
            view->vx = 3;
        }
        if (view->vy == 0) {
            view->vy = -3;
        }

        lv_obj_set_pos(
            view->body,
            view->x,
            view->y);

        const dice_set_face_t *preview =
            dice_custom_random_face(
                rolled->die,
                NULL);

        if (preview != NULL) {
            apply_custom_face(
                view,
                set,
                rolled->die,
                preview);
        }
    }

    lv_obj_move_foreground(s_input_layer);
    s_animation_timer = lv_timer_create(
        animation_callback,
        ANIMATION_TIMER_MS,
        NULL);

    bsp_display_unlock();
}


void dice_ui_start_custom_reroll_animation(
    const dice_set_definition_t *set,
    const dice_custom_roll_result_t *result,
    const bool *rerolled_mask,
    size_t rerolled_mask_size)
{
    if (set == NULL ||
        result == NULL ||
        result->count == 0 ||
        rerolled_mask == NULL ||
        !bsp_display_lock(500)) {
        return;
    }

    s_custom_mode = true;
    s_custom_set = set;

    register_activity();
    cancel_selector_hide();
    lv_obj_add_flag(s_selector, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_menu, LV_OBJ_FLAG_HIDDEN);
    s_menu_visible = false;
    clear_die_views();

    s_custom_final_result = *result;
    s_die_view_count = 0;
    lv_obj_add_flag(
        s_custom_action_button,
        LV_OBJ_FLAG_HIDDEN);

    dice_power_high_performance_acquire();
    s_rolling = true;
    s_roll_started_us = esp_timer_get_time();
    s_last_face_change_us = s_roll_started_us;

    lv_label_set_text(
        s_result_label,
        "Rerolling failures...");
    lv_label_set_text(
        s_hint_label,
        "Kept dice remain fixed");

    int unit_size =
        animation_unit_size(result->count);

    /*
     * Create kept dice first and place them statically in the upper-left
     * portion of the tray.
     */
    int kept_x = 6;
    int kept_y = TRAY_TOP + 8;
    int kept_row_height = 0;

    for (size_t result_index = 0;
         result_index < result->count;
         ++result_index) {
        bool rerolled =
            result_index < rerolled_mask_size &&
            rerolled_mask[result_index];

        if (rerolled) {
            continue;
        }

        die_view_t *view =
            &s_die_views[s_die_view_count++];
        const dice_custom_result_die_t *rolled =
            &s_custom_final_result.dice[result_index];

        view->animated = false;
        view->result_index = result_index;

        create_custom_die_graphics(
            view,
            unit_size,
            rolled->die_index,
            rolled->die);

        if (kept_x + view->width >
            SCREEN_WIDTH - 6) {
            kept_x = 6;
            kept_y += kept_row_height + SPAWN_GAP;
            kept_row_height = 0;
        }

        view->x = kept_x;
        view->y = kept_y;
        view->vx = 0;
        view->vy = 0;

        apply_custom_face(
            view,
            set,
            rolled->die,
            rolled->face);

        lv_obj_set_pos(
            view->body,
            view->x,
            view->y);

        kept_x += view->width + SPAWN_GAP;
        if (view->height > kept_row_height) {
            kept_row_height = view->height;
        }
    }

    /*
     * Create only the rerolled dice as moving dice.
     */
    for (size_t result_index = 0;
         result_index < result->count;
         ++result_index) {
        bool rerolled =
            result_index < rerolled_mask_size &&
            rerolled_mask[result_index];

        if (!rerolled) {
            continue;
        }

        die_view_t *view =
            &s_die_views[s_die_view_count++];
        const dice_custom_result_die_t *rolled =
            &s_custom_final_result.dice[result_index];

        view->animated = true;
        view->result_index = result_index;

        create_custom_die_graphics(
            view,
            unit_size,
            rolled->die_index,
            rolled->die);

        choose_spawn_position(
            view,
            s_die_view_count - 1);

        view->vx = (int)(esp_random() % 9) - 4;
        view->vy = (int)(esp_random() % 9) - 4;

        if (view->vx == 0) {
            view->vx = 3;
        }
        if (view->vy == 0) {
            view->vy = -3;
        }

        lv_obj_set_pos(
            view->body,
            view->x,
            view->y);

        const dice_set_face_t *preview =
            dice_custom_random_face(
                rolled->die,
                NULL);

        if (preview != NULL) {
            apply_custom_face(
                view,
                set,
                rolled->die,
                preview);
        }
    }

    lv_obj_move_foreground(s_input_layer);
    s_animation_timer = lv_timer_create(
        animation_callback,
        ANIMATION_TIMER_MS,
        NULL);

    bsp_display_unlock();
}

static void layout_rule_menu_rows(void)
{
    int next_y = 0;

    if (s_rule_clear_button != NULL) {
        lv_obj_set_pos(
            s_rule_clear_button,
            4,
            next_y);
        next_y += RULE_OPTION_ROW_HEIGHT;
    }

    if (s_custom_action_button != NULL) {
        if (s_custom_action_visible) {
            lv_obj_set_pos(
                s_custom_action_button,
                4,
                next_y);
            lv_obj_clear_flag(
                s_custom_action_button,
                LV_OBJ_FLAG_HIDDEN);
            next_y += RULE_OPTION_ROW_HEIGHT;
        } else {
            lv_obj_add_flag(
                s_custom_action_button,
                LV_OBJ_FLAG_HIDDEN);
        }
    }

    for (size_t index = 0;
         index < s_rule_option_count;
         ++index) {
        if (s_rule_option_buttons[index] == NULL) {
            continue;
        }

        lv_obj_set_pos(
            s_rule_option_buttons[index],
            4,
            next_y);
        next_y += RULE_OPTION_ROW_HEIGHT;
    }

    lv_obj_update_layout(
        s_rule_menu_list);
}

void dice_ui_activate_standard_set(
    const dice_pool_t *pool)
{
    if (!bsp_display_lock(250)) {
        return;
    }

    s_custom_mode = false;
    s_custom_set = NULL;
    s_selected_item = DICE_D20;
    s_custom_action_visible = false;
    s_custom_result_title[0] = '\0';
    s_custom_result_details[0] = '\0';
    if (s_custom_action_button != NULL) {
        lv_obj_add_flag(s_custom_action_button, LV_OBJ_FLAG_HIDDEN);
    }
    layout_rule_menu_rows();

    if (pool != NULL) {
        s_pool = *pool;
    }

    clear_die_views();
    dice_face_assets_release_all();
    update_pool_text();
    update_selector_text();
    lv_label_set_text(s_result_label, "Ready");
    lv_label_set_text(
        s_hint_label,
        "Touch to choose dice, then shake");

    bsp_display_unlock();
}

void dice_ui_activate_custom_set(
    const dice_set_definition_t *set,
    const dice_custom_pool_t *pool)
{
    if (set == NULL ||
        set->built_in ||
        set->die_count == 0 ||
        !bsp_display_lock(250)) {
        return;
    }

    s_custom_mode = true;
    s_custom_set = set;
    s_selected_item = 0;
    s_custom_action_visible = false;
    s_custom_result_title[0] = '\0';
    s_custom_result_details[0] = '\0';
    if (s_custom_action_button != NULL) {
        lv_obj_add_flag(s_custom_action_button, LV_OBJ_FLAG_HIDDEN);
    }
    layout_rule_menu_rows();

    if (pool != NULL) {
        s_custom_pool = *pool;
    } else {
        dice_custom_pool_clear(&s_custom_pool);
    }

    clear_die_views();
    if (s_startup_preloaded_set != set) {
        dice_face_assets_release_all();
        dice_face_assets_preload_set(set);
    }
    s_startup_preloaded_set = NULL;
    update_pool_text();
    update_selector_text();

    char title[64];
    snprintf(
        title,
        sizeof(title),
        "%.47s",
        set->name);
    lv_label_set_text(s_result_label, title);
    lv_label_set_text(
        s_hint_label,
        "Touch to choose dice, then shake");

    bsp_display_unlock();
}

void dice_ui_set_custom_pool(
    const dice_custom_pool_t *pool)
{
    if (pool == NULL || !bsp_display_lock(250)) {
        return;
    }

    s_custom_pool = *pool;
    update_pool_text();
    update_selector_text();
    bsp_display_unlock();
}



void dice_ui_set_rule_options(
    const dice_ui_rule_option_t *options,
    size_t count)
{
    if (!bsp_display_lock(500)) {
        return;
    }

    if (count > DICE_UI_MAX_RULE_OPTIONS) {
        count = DICE_UI_MAX_RULE_OPTIONS;
    }

    for (size_t index = 0;
         index < DICE_UI_MAX_RULE_OPTIONS;
         ++index) {
        if (s_rule_option_buttons[index] != NULL) {
            lv_obj_delete(
                s_rule_option_buttons[index]);
        }

        s_rule_option_buttons[index] = NULL;
        s_rule_option_labels[index] = NULL;
        s_rule_option_minus_buttons[index] = NULL;
        s_rule_option_plus_buttons[index] = NULL;
    }

    s_rule_option_count = count;

    for (size_t index = 0;
         index < count;
         ++index) {
        s_rule_options[index] = options[index];

        lv_obj_t *row =
            lv_obj_create(s_rule_menu_list);
        lv_obj_set_size(
            row,
            RULE_MENU_ROW_WIDTH,
            RULE_MENU_ROW_HEIGHT);
        lv_obj_set_pos(
            row,
            4,
            0);
        lv_obj_set_style_pad_all(
            row,
            0,
            LV_PART_MAIN);
        style_unified_menu_button(row);
        lv_obj_set_style_radius(
            row,
            18,
            LV_PART_MAIN);
        lv_obj_clear_flag(
            row,
            LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(
            row,
            LV_OBJ_FLAG_GESTURE_BUBBLE);

        lv_obj_t *minus = NULL;
        lv_obj_t *plus = NULL;

        if (options[index].toggle) {
            lv_obj_add_event_cb(
                row,
                rule_option_toggle_callback,
                LV_EVENT_CLICKED,
                (void *)(uintptr_t)index);
        } else {
            /*
             * Invisible half-row hit areas. Every point on the left half
             * decrements and every point on the right half increments.
             */
            minus = lv_obj_create(row);
            lv_obj_set_size(
                minus,
                RULE_MENU_ROW_WIDTH / 2,
                RULE_MENU_ROW_HEIGHT);
            lv_obj_align(
                minus,
                LV_ALIGN_LEFT_MID,
                0,
                0);
            lv_obj_set_style_bg_opa(
                minus,
                LV_OPA_TRANSP,
                LV_PART_MAIN);
            lv_obj_set_style_border_width(
                minus,
                0,
                LV_PART_MAIN);
            lv_obj_set_style_pad_all(
                minus,
                0,
                LV_PART_MAIN);
            lv_obj_clear_flag(
                minus,
                LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_flag(
                minus,
                LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(
                minus,
                rule_option_adjust_callback,
                LV_EVENT_CLICKED,
                (void *)(uintptr_t)
                    (index << 1));

            plus = lv_obj_create(row);
            lv_obj_set_size(
                plus,
                RULE_MENU_ROW_WIDTH / 2,
                RULE_MENU_ROW_HEIGHT);
            lv_obj_align(
                plus,
                LV_ALIGN_RIGHT_MID,
                0,
                0);
            lv_obj_set_style_bg_opa(
                plus,
                LV_OPA_TRANSP,
                LV_PART_MAIN);
            lv_obj_set_style_border_width(
                plus,
                0,
                LV_PART_MAIN);
            lv_obj_set_style_pad_all(
                plus,
                0,
                LV_PART_MAIN);
            lv_obj_clear_flag(
                plus,
                LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_flag(
                plus,
                LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(
                plus,
                rule_option_adjust_callback,
                LV_EVENT_CLICKED,
                (void *)(uintptr_t)
                    ((index << 1) | 1u));

            lv_obj_t *minus_label =
                lv_label_create(row);
            lv_label_set_text(
                minus_label,
                "-");
            lv_obj_set_style_text_font(
                minus_label,
                &lv_font_montserrat_32,
                LV_PART_MAIN);
            lv_obj_set_style_text_color(
                minus_label,
                lv_color_hex(0xff73cf),
                LV_PART_MAIN);
            lv_obj_align(
                minus_label,
                LV_ALIGN_LEFT_MID,
                22,
                10);

            lv_obj_t *plus_label =
                lv_label_create(row);
            lv_label_set_text(
                plus_label,
                "+");
            lv_obj_set_style_text_font(
                plus_label,
                &lv_font_montserrat_32,
                LV_PART_MAIN);
            lv_obj_set_style_text_color(
                plus_label,
                lv_color_hex(0xff73cf),
                LV_PART_MAIN);
            lv_obj_align(
                plus_label,
                LV_ALIGN_RIGHT_MID,
                -22,
                10);
        }

        lv_obj_t *label =
            lv_label_create(row);
        char row_text[56];
        snprintf(
            row_text,
            sizeof(row_text),
            "%.31s\n%.15s",
            options[index].label,
            options[index].value);
        lv_label_set_text(
            label,
            row_text);
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
        lv_obj_set_size(
            label,
            options[index].toggle ? 220 : 170,
            68);
        lv_obj_set_style_text_font(
            label,
            options[index].toggle ? &lv_font_montserrat_24 : &lv_font_montserrat_18,
            LV_PART_MAIN);
        lv_obj_set_style_text_color(
            label,
            lv_color_hex(0xffffff),
            LV_PART_MAIN);
        lv_obj_set_style_text_align(
            label,
            LV_TEXT_ALIGN_CENTER,
            LV_PART_MAIN);
        lv_obj_align(
            label,
            LV_ALIGN_CENTER,
            0,
            0);

        s_rule_option_buttons[index] = row;
        s_rule_option_labels[index] = label;
        s_rule_option_minus_buttons[index] = minus;
        s_rule_option_plus_buttons[index] = plus;
    }

    layout_rule_menu_rows();

    bsp_display_unlock();
}

void dice_ui_show_rule_options(bool visible)
{
    if (!bsp_display_lock(500)) {
        return;
    }

    register_activity();
    cancel_selector_hide();
    lv_obj_add_flag(
        s_selector,
        LV_OBJ_FLAG_HIDDEN);

    if (visible) {
        s_menu_visible = true;
        lv_obj_add_flag(
            s_input_layer,
            LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(
            s_menu,
            LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(
            s_set_menu,
            LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(
            s_rule_menu,
            LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(
            s_rule_menu);
        lv_obj_scroll_to_y(
            s_rule_menu_list,
            0,
            LV_ANIM_OFF);
    } else {
        close_rule_menu_from_ui();
    }

    bsp_display_unlock();
}

void dice_ui_set_custom_result_text(const char *title, const char *details)
{
    snprintf(s_custom_result_title, sizeof(s_custom_result_title), "%.63s",
        title != NULL ? title : "");
    snprintf(s_custom_result_details, sizeof(s_custom_result_details), "%.95s",
        details != NULL ? details : "");
}

void dice_ui_set_main_rule_option(
    const char *label,
    const char *value,
    bool visible,
    size_t option_index)
{
    (void)label;
    (void)value;
    (void)visible;
    (void)option_index;

    s_main_option_visible = false;

    if (s_main_option_button != NULL &&
        bsp_display_lock(250)) {
        lv_obj_add_flag(
            s_main_option_button,
            LV_OBJ_FLAG_HIDDEN);
        bsp_display_unlock();
    }
}

void dice_ui_set_custom_action(
    const char *label,
    bool visible,
    size_t action_index)
{
    if (!bsp_display_lock(500)) {
        ESP_LOGW(
            TAG,
            "Could not lock display while updating custom action");
        return;
    }

    s_custom_action_visible = visible;
    s_custom_action_index = action_index;

    if (s_custom_action_label != NULL) {
        lv_label_set_text(
            s_custom_action_label,
            label != NULL ? label : "");
        lv_obj_center(s_custom_action_label);
    }

    layout_rule_menu_rows();

    bsp_display_unlock();
}

bool dice_ui_is_rolling(void)
{
    return s_rolling;
}

bool dice_ui_is_overlay_visible(void)
{
    return s_menu_visible ||
        (s_rule_menu != NULL &&
         !lv_obj_has_flag(
             s_rule_menu,
             LV_OBJ_FLAG_HIDDEN)) ||
        (s_set_menu != NULL &&
         !lv_obj_has_flag(
             s_set_menu,
             LV_OBJ_FLAG_HIDDEN)) ||
        (s_about_panel != NULL &&
         !lv_obj_has_flag(
             s_about_panel,
             LV_OBJ_FLAG_HIDDEN));
}


void dice_ui_show_menu(bool visible)
{
    if (!bsp_display_lock(250)) {
        return;
    }

    register_activity();
    s_menu_visible = visible;
    if (!visible && s_about_panel != NULL) {
        lv_obj_add_flag(s_about_panel, LV_OBJ_FLAG_HIDDEN);
    }
    cancel_selector_hide();
    lv_obj_add_flag(s_selector, LV_OBJ_FLAG_HIDDEN);

    if (visible) {
        s_menu_scroll_offset = 0;
        update_menu_scroll_position();
        lv_obj_add_flag(s_input_layer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_set_menu, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(
        s_set_menu,
        set_menu_swipe_callback,
        LV_EVENT_PRESSED,
        NULL);
    lv_obj_add_event_cb(
        s_set_menu,
        set_menu_swipe_callback,
        LV_EVENT_RELEASED,
        NULL);
        lv_obj_add_flag(s_rule_menu, LV_OBJ_FLAG_HIDDEN);
        if (s_main_option_button != NULL) {
            lv_obj_add_flag(
                s_main_option_button,
                LV_OBJ_FLAG_HIDDEN);
        }
        lv_obj_clear_flag(s_menu, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_menu);
    } else {
        lv_obj_add_flag(s_menu, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_set_menu, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(
        s_set_menu,
        set_menu_swipe_callback,
        LV_EVENT_PRESSED,
        NULL);
    lv_obj_add_event_cb(
        s_set_menu,
        set_menu_swipe_callback,
        LV_EVENT_RELEASED,
        NULL);
        lv_obj_add_flag(s_rule_menu, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_input_layer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_input_layer);

        if (s_main_option_visible &&
            s_main_option_button != NULL) {
            lv_obj_clear_flag(
                s_main_option_button,
                LV_OBJ_FLAG_HIDDEN);
        }
    }

    bsp_display_unlock();
}


void dice_ui_set_available_sets(
    const char *const *names,
    size_t count,
    size_t active_index)
{
    if (!bsp_display_lock(500)) {
        return;
    }

    if (count > SET_MENU_MAX_SETS) {
        count = SET_MENU_MAX_SETS;
    }

    for (size_t index = 0;
         index < SET_MENU_MAX_SETS;
         ++index) {
        if (s_set_menu_buttons[index] != NULL) {
            lv_obj_delete(s_set_menu_buttons[index]);
            s_set_menu_buttons[index] = NULL;
            s_set_menu_labels[index] = NULL;
        }
    }

    s_set_count = count;
    s_active_set_index =
        active_index < count ? active_index : 0;

    for (size_t index = 0;
         index < count;
         ++index) {
        snprintf(
            s_set_names[index],
            sizeof(s_set_names[index]),
            "%s",
            names[index] != NULL
                ? names[index]
                : "Unnamed");

        create_set_button(s_set_menu_list, index);

        char label[64] = {0};

        if (index == s_active_set_index) {
            label[0] = '*';
            label[1] = ' ';
            snprintf(
                label + 2,
                sizeof(label) - 2,
                "%.47s",
                s_set_names[index]);
        } else {
            snprintf(
                label,
                sizeof(label),
                "%.47s",
                s_set_names[index]);
        }

        lv_label_set_text(
            s_set_menu_labels[index],
            label);

        lv_obj_set_style_bg_color(
            s_set_menu_buttons[index],
            index == s_active_set_index
                ? lv_color_hex(0x1578a8)
                : lv_color_hex(0x0d4c70),
            LV_PART_MAIN);
        lv_obj_set_style_border_color(
            s_set_menu_buttons[index],
            index == s_active_set_index
                ? lv_color_hex(0xffdc68)
                : lv_color_hex(0x70d7ff),
            LV_PART_MAIN);
    }

    lv_obj_set_height(
        s_set_menu_list,
        350);

    char main_label[64];
    snprintf(
        main_label,
        sizeof(main_label),
        "Set: %s",
        count > 0
            ? s_set_names[s_active_set_index]
            : "Standard");
    lv_label_set_text(s_menu_set_label, main_label);

    bsp_display_unlock();
}

void dice_ui_show_set_menu(bool visible)
{
    if (!bsp_display_lock(250)) {
        return;
    }

    register_activity();

    if (visible) {
        if (s_main_option_button != NULL) {
            lv_obj_add_flag(
                s_main_option_button,
                LV_OBJ_FLAG_HIDDEN);
        }
        lv_obj_add_flag(s_menu, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_set_menu, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_set_menu);
        lv_obj_scroll_to_y(
            s_set_menu_list,
            0,
            LV_ANIM_OFF);
    } else {
        if (s_main_option_visible &&
            s_main_option_button != NULL &&
            !s_menu_visible) {
            lv_obj_clear_flag(
                s_main_option_button,
                LV_OBJ_FLAG_HIDDEN);
        }
        lv_obj_add_flag(s_set_menu, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(
        s_set_menu,
        set_menu_swipe_callback,
        LV_EVENT_PRESSED,
        NULL);
    lv_obj_add_event_cb(
        s_set_menu,
        set_menu_swipe_callback,
        LV_EVENT_RELEASED,
        NULL);
        lv_obj_add_flag(s_rule_menu, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_menu, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_menu);
    }

    bsp_display_unlock();
}

void dice_ui_show_notice(const char *message)
{
    if (message == NULL || !bsp_display_lock(250)) {
        return;
    }
    show_notice_locked(message);
    bsp_display_unlock();
}

void dice_ui_show_about_status(const char *status_text)
{
    if (status_text == NULL ||
        s_about_panel == NULL ||
        s_about_content_label == NULL ||
        !bsp_display_lock(250)) {
        return;
    }

    register_activity();
    lv_label_set_text(s_about_content_label, status_text);
    lv_obj_scroll_to_y(
        lv_obj_get_parent(s_about_content_label),
        0,
        LV_ANIM_OFF);
    lv_obj_add_flag(s_menu, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_about_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_about_panel);
    bsp_display_unlock();
}

void dice_ui_show_pool_cleared(void)
{
    if (!bsp_display_lock(250)) {
        return;
    }

    cancel_selector_hide();
    lv_obj_add_flag(s_selector, LV_OBJ_FLAG_HIDDEN);
    show_notice_locked("Dice pool cleared");
    bsp_display_unlock();
}

void dice_ui_set_battery_status(
    bool available,
    uint16_t voltage_mv,
    uint8_t percent)
{
    if (!bsp_display_lock(250)) {
        return;
    }

    char text[48];

    if (available) {
        snprintf(
            text,
            sizeof(text),
            "BATTERY  %u.%02u V  %u%%",
            (unsigned)(voltage_mv / 1000),
            (unsigned)((voltage_mv % 1000) / 10),
            (unsigned)percent);
    } else {
        snprintf(
            text,
            sizeof(text),
            "BATTERY  --.-- V  --%%");
    }

    lv_label_set_text(s_menu_battery_label, text);
    bsp_display_unlock();
}

void dice_ui_set_menu_values(
    bool sound_enabled,
    int brightness_percent,
    int shake_level,
    int timeout_seconds,
    int selector_hide_ms)
{
    if (!bsp_display_lock(250)) {
        return;
    }

    s_menu_sound_enabled = sound_enabled;
    s_menu_brightness = brightness_percent;
    s_menu_shake_level = shake_level;
    s_menu_timeout_seconds = timeout_seconds;
    s_menu_selector_hide_ms = selector_hide_ms;

    char text[64];
    snprintf(
        text, sizeof(text), "Sound: %s",
        sound_enabled ? "On" : "Off");
    lv_label_set_text(s_menu_sound_label, text);

    snprintf(
        text, sizeof(text), "Brightness: %d%%",
        brightness_percent);
    lv_label_set_text(s_menu_brightness_label, text);

    static const char *shake_names[] = {
        "Gentle", "Normal", "Firm"
    };
    int safe_level = shake_level;
    if (safe_level < 0) safe_level = 0;
    if (safe_level > 2) safe_level = 2;
    snprintf(
        text, sizeof(text), "Shake sensitivity: %s",
        shake_names[safe_level]);
    lv_label_set_text(s_menu_shake_label, text);

    if (timeout_seconds <= 0) {
        snprintf(text, sizeof(text), "Display timeout\nOff");
    } else {
        snprintf(
            text, sizeof(text), "Display timeout\n%d sec",
            timeout_seconds);
    }
    lv_label_set_text(s_menu_timeout_label, text);

    snprintf(
        text,
        sizeof(text),
        "Selector return\n%d.%d sec",
        selector_hide_ms / 1000,
        (selector_hide_ms % 1000) / 100);
    lv_label_set_text(s_menu_selector_delay_label, text);

    bsp_display_unlock();
}
