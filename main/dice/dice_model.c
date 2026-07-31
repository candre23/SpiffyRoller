#include "dice_model.h"

#include <string.h>

#include "esp_random.h"

static const int16_t s_d4_faces[] = {1, 2, 3, 4};
static const int16_t s_d6_faces[] = {1, 2, 3, 4, 5, 6};
static const int16_t s_d8_faces[] = {1, 2, 3, 4, 5, 6, 7, 8};
static const int16_t s_d10_faces[] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10
};
static const int16_t s_d12_faces[] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12
};
static const int16_t s_d20_faces[] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20
};

static const dice_definition_t s_definitions[DICE_TYPE_COUNT] = {
    {DICE_D4, "d4", "D4", 4, s_d4_faces, false},
    {DICE_D6, "d6", "D6", 6, s_d6_faces, false},
    {DICE_D8, "d8", "D8", 8, s_d8_faces, false},
    {DICE_D10, "d10", "D10", 10, s_d10_faces, false},
    {DICE_D12, "d12", "D12", 12, s_d12_faces, false},
    {DICE_D20, "d20", "D20", 20, s_d20_faces, false},
    {DICE_PERCENTILE, "percentile", "D%", 100, NULL, true},
};

static uint32_t random_below(uint32_t limit)
{
    if (limit <= 1) {
        return 0;
    }

    uint32_t cutoff = UINT32_MAX - (UINT32_MAX % limit);
    uint32_t value;

    do {
        value = esp_random();
    } while (value >= cutoff);

    return value % limit;
}

static void make_standard_result(
    const dice_definition_t *definition,
    die_result_t *result)
{
    uint16_t face = (uint16_t)random_below(definition->side_count);
    int16_t value = definition->face_values[face];

    result->definition = definition;
    result->kind = DIE_RESULT_STANDARD;
    result->face_index = face;
    result->secondary_face_index = 0;
    result->numeric_value = value;
    result->display_primary = value;
    result->display_secondary = -1;
}

static void make_percentile_result(
    const dice_definition_t *definition,
    die_result_t *result)
{
    uint16_t tens_face = (uint16_t)random_below(10);
    uint16_t ones_face = (uint16_t)random_below(10);
    int16_t tens = (int16_t)(tens_face * 10);
    int16_t ones = (int16_t)ones_face;
    int16_t value = (int16_t)(tens + ones);

    if (value == 0) {
        value = 100;
    }

    result->definition = definition;
    result->kind = DIE_RESULT_PERCENTILE;
    result->face_index = tens_face;
    result->secondary_face_index = ones_face;
    result->numeric_value = value;
    result->display_primary = tens;
    result->display_secondary = ones;
}

void dice_model_init_default_pool(dice_pool_t *pool)
{
    if (pool == NULL) {
        return;
    }

    memset(pool, 0, sizeof(*pool));
    pool->quantity[DICE_D20] = 1;
}

const dice_definition_t *dice_model_get_definition(dice_type_id_t type)
{
    if (type < 0 || type >= DICE_TYPE_COUNT) {
        return NULL;
    }

    return &s_definitions[type];
}

const char *dice_model_type_label(dice_type_id_t type)
{
    const dice_definition_t *definition = dice_model_get_definition(type);
    return definition != NULL ? definition->label : "?";
}

uint16_t dice_model_get_quantity(
    const dice_pool_t *pool,
    dice_type_id_t type)
{
    if (pool == NULL || type < 0 || type >= DICE_TYPE_COUNT) {
        return 0;
    }

    return pool->quantity[type];
}

size_t dice_model_pool_count(const dice_pool_t *pool)
{
    if (pool == NULL) {
        return 0;
    }

    size_t count = 0;
    for (int type = 0; type < DICE_TYPE_COUNT; ++type) {
        count += pool->quantity[type];
    }

    return count;
}

size_t dice_model_physical_die_count(const dice_pool_t *pool)
{
    if (pool == NULL) {
        return 0;
    }

    size_t count = dice_model_pool_count(pool);
    if (pool->quantity[DICE_PERCENTILE] > 0) {
        ++count;
    }

    return count;
}

void dice_model_adjust_quantity(
    dice_pool_t *pool,
    dice_type_id_t type,
    int delta)
{
    if (pool == NULL || type < 0 || type >= DICE_TYPE_COUNT ||
        delta == 0) {
        return;
    }

    int32_t desired = (int32_t)pool->quantity[type] + delta;

    if (type == DICE_PERCENTILE) {
        desired = desired > 0 ? 1 : 0;
    } else {
        if (desired < 0) {
            desired = 0;
        }
        if (desired > UINT16_MAX) {
            desired = UINT16_MAX;
        }
    }


    pool->quantity[type] = (uint16_t)desired;
}

void dice_model_make_preview(
    const dice_definition_t *definition,
    die_result_t *preview)
{
    if (definition == NULL || preview == NULL) {
        return;
    }

    memset(preview, 0, sizeof(*preview));

    if (definition->type == DICE_PERCENTILE) {
        make_percentile_result(definition, preview);
    } else {
        make_standard_result(definition, preview);
    }
}

bool dice_model_roll(
    const dice_pool_t *pool,
    dice_roll_result_t *result)
{
    if (pool == NULL || result == NULL) {
        return false;
    }

    memset(result, 0, sizeof(*result));

    for (int type = 0; type < DICE_TYPE_COUNT; ++type) {
        const dice_definition_t *definition = &s_definitions[type];

        for (uint16_t die = 0; die < pool->quantity[type]; ++die) {
            die_result_t generated;
            dice_model_make_preview(definition, &generated);

            result->numeric_total += generated.numeric_value;
            ++result->total_count;

            if (result->count < DICE_MAX_TOTAL) {
                result->dice[result->count++] = generated;
            }
        }
    }

    return result->total_count > 0;
}

static int compare_results(
    const die_result_t *left,
    const die_result_t *right)
{
    if (left->definition->type != right->definition->type) {
        return (int)left->definition->type -
               (int)right->definition->type;
    }

    return (int)right->numeric_value - (int)left->numeric_value;
}

void dice_model_sort_result(dice_roll_result_t *result)
{
    if (result == NULL) {
        return;
    }

    for (size_t index = 1; index < result->count; ++index) {
        die_result_t key = result->dice[index];
        size_t previous = index;

        while (previous > 0 &&
               compare_results(&result->dice[previous - 1], &key) > 0) {
            result->dice[previous] = result->dice[previous - 1];
            --previous;
        }

        result->dice[previous] = key;
    }
}
