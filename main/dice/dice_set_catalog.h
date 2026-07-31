#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DICE_SET_MAX_SETS 16
#define DICE_SET_MAX_DICE 16
#define DICE_SET_MAX_FACES 64
#define DICE_SET_ID_MAX 31
#define DICE_SET_NAME_MAX 47
#define DICE_SET_PATH_MAX 127
#define DICE_SET_SHAPE_MAX 15
#define DICE_SET_LABEL_MAX 31

typedef enum {
    DICE_FACE_ART_NONE = 0,
    DICE_FACE_ART_TEXT,
    DICE_FACE_ART_MASK,
    DICE_FACE_ART_INDEXED,
} dice_face_art_mode_t;

typedef enum {
    DICE_FACE_DISPLAY_BLANK = 0,
    DICE_FACE_DISPLAY_LABEL,
    DICE_FACE_DISPLAY_VALUE,
    DICE_FACE_DISPLAY_IMAGE,
} dice_face_display_mode_t;

typedef struct {
    int32_t numeric_value;
    bool has_numeric_value;
    char label[DICE_SET_LABEL_MAX + 1];
    char image_path[DICE_SET_PATH_MAX + 1];
    dice_face_display_mode_t display_mode;
    dice_face_art_mode_t art_mode;

    uint8_t success_count;
    uint8_t advantage_count;
    uint8_t triumph_count;
    uint8_t failure_count;
    uint8_t threat_count;
    uint8_t despair_count;
    bool has_symbol_values;

    uint32_t body_color;
    uint32_t ink_color;
    bool has_body_color_override;
    bool has_ink_color_override;
} dice_set_face_t;

typedef struct {
    char id[DICE_SET_ID_MAX + 1];
    char name[DICE_SET_NAME_MAX + 1];
    char shape[DICE_SET_SHAPE_MAX + 1];
    uint16_t side_count;

    uint32_t body_color;
    uint32_t ink_color;

    dice_set_face_t faces[DICE_SET_MAX_FACES];
    size_t face_count;
} dice_set_die_t;

typedef struct {
    char id[DICE_SET_ID_MAX + 1];
    char name[DICE_SET_NAME_MAX + 1];
    char folder_path[DICE_SET_PATH_MAX + 1];
    char rules_path[DICE_SET_PATH_MAX + 1];
    uint16_t format_version;
    bool built_in;
    bool valid;

    dice_set_die_t dice[DICE_SET_MAX_DICE];
    size_t die_count;
} dice_set_definition_t;

bool dice_set_catalog_scan(void);
size_t dice_set_catalog_count(void);
const dice_set_definition_t *dice_set_catalog_get(size_t index);
const dice_set_definition_t *dice_set_catalog_find(const char *id);
size_t dice_set_catalog_index_of(const char *id);
