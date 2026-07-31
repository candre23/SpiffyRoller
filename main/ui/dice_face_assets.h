#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dice_set_catalog.h"
#include "lvgl.h"

typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t *rgba;
    size_t rgba_size;
} dice_face_bitmap_t;

bool dice_face_assets_get(
    const char *absolute_path,
    const dice_face_bitmap_t **out_bitmap);

bool dice_face_assets_get_rendered(
    const char *absolute_path,
    dice_face_art_mode_t art_mode,
    uint32_t ink_color,
    int target_width,
    int target_height,
    const lv_image_dsc_t **out_image);

void dice_face_assets_preload_set(
    const dice_set_definition_t *set);

void dice_face_assets_release_all(void);

bool dice_face_assets_render(
    const dice_face_bitmap_t *bitmap,
    dice_face_art_mode_t art_mode,
    uint32_t ink_color,
    int target_width,
    int target_height,
    lv_color32_t *out_pixels);
