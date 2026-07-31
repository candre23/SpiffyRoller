#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dice_set_catalog.h"

#define DICE_CUSTOM_MAX_TOTAL 64

typedef struct {
    uint16_t quantity[DICE_SET_MAX_DICE];
} dice_custom_pool_t;

typedef struct {
    const dice_set_die_t *die;
    const dice_set_face_t *face;
    size_t die_index;
    size_t face_index;
} dice_custom_result_die_t;

typedef struct {
    dice_custom_result_die_t dice[DICE_CUSTOM_MAX_TOTAL];
    size_t count;
    size_t total_count;
    int32_t numeric_total;
} dice_custom_roll_result_t;

void dice_custom_pool_clear(dice_custom_pool_t *pool);
uint16_t dice_custom_pool_get(
    const dice_custom_pool_t *pool,
    size_t die_index);
void dice_custom_pool_adjust(
    dice_custom_pool_t *pool,
    size_t die_index,
    int delta);
size_t dice_custom_pool_count(
    const dice_custom_pool_t *pool,
    size_t die_count);

bool dice_custom_roll(
    const dice_set_definition_t *set,
    const dice_custom_pool_t *pool,
    dice_custom_roll_result_t *result);

const dice_set_face_t *dice_custom_random_face(
    const dice_set_die_t *die,
    size_t *face_index);
