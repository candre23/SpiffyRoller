#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DICE_TYPE_COUNT 7
#define DICE_MAX_TOTAL 64

typedef enum {
    DICE_D4 = 0,
    DICE_D6,
    DICE_D8,
    DICE_D10,
    DICE_D12,
    DICE_D20,
    DICE_PERCENTILE,
} dice_type_id_t;

typedef enum {
    DIE_RESULT_STANDARD = 0,
    DIE_RESULT_PERCENTILE,
} die_result_kind_t;

typedef struct {
    dice_type_id_t type;
    const char *id;
    const char *label;
    uint16_t side_count;
    const int16_t *face_values;
    bool compound;
} dice_definition_t;

typedef struct {
    uint16_t quantity[DICE_TYPE_COUNT];
} dice_pool_t;

typedef struct {
    const dice_definition_t *definition;
    die_result_kind_t kind;
    uint16_t face_index;
    uint16_t secondary_face_index;
    int16_t numeric_value;
    int16_t display_primary;
    int16_t display_secondary;
} die_result_t;

typedef struct {
    die_result_t dice[DICE_MAX_TOTAL];
    size_t count;
    size_t total_count;
    int32_t numeric_total;
} dice_roll_result_t;

void dice_model_init_default_pool(dice_pool_t *pool);
const dice_definition_t *dice_model_get_definition(dice_type_id_t type);
const char *dice_model_type_label(dice_type_id_t type);
uint16_t dice_model_get_quantity(
    const dice_pool_t *pool,
    dice_type_id_t type);
void dice_model_adjust_quantity(
    dice_pool_t *pool,
    dice_type_id_t type,
    int delta);
size_t dice_model_pool_count(const dice_pool_t *pool);
size_t dice_model_physical_die_count(const dice_pool_t *pool);
bool dice_model_roll(
    const dice_pool_t *pool,
    dice_roll_result_t *result);
void dice_model_make_preview(
    const dice_definition_t *definition,
    die_result_t *preview);
void dice_model_sort_result(dice_roll_result_t *result);
