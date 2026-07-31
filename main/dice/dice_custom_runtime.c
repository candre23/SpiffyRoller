#include "dice_custom_runtime.h"

#include <string.h>

#include "esp_random.h"

static uint32_t random_below(uint32_t limit)
{
    if (limit <= 1) {
        return 0;
    }

    uint32_t cutoff =
        UINT32_MAX - (UINT32_MAX % limit);
    uint32_t value;

    do {
        value = esp_random();
    } while (value >= cutoff);

    return value % limit;
}

void dice_custom_pool_clear(dice_custom_pool_t *pool)
{
    if (pool != NULL) {
        memset(pool, 0, sizeof(*pool));
    }
}

uint16_t dice_custom_pool_get(
    const dice_custom_pool_t *pool,
    size_t die_index)
{
    if (pool == NULL ||
        die_index >= DICE_SET_MAX_DICE) {
        return 0;
    }

    return pool->quantity[die_index];
}

void dice_custom_pool_adjust(
    dice_custom_pool_t *pool,
    size_t die_index,
    int delta)
{
    if (pool == NULL ||
        die_index >= DICE_SET_MAX_DICE ||
        delta == 0) {
        return;
    }

    int32_t desired =
        (int32_t)pool->quantity[die_index] +
        delta;

    if (desired < 0) {
        desired = 0;
    }
    if (desired > UINT16_MAX) {
        desired = UINT16_MAX;
    }

    pool->quantity[die_index] =
        (uint16_t)desired;
}

size_t dice_custom_pool_count(
    const dice_custom_pool_t *pool,
    size_t die_count)
{
    if (pool == NULL) {
        return 0;
    }

    if (die_count > DICE_SET_MAX_DICE) {
        die_count = DICE_SET_MAX_DICE;
    }

    size_t count = 0;

    for (size_t index = 0;
         index < die_count;
         ++index) {
        count += pool->quantity[index];
    }

    return count;
}

const dice_set_face_t *dice_custom_random_face(
    const dice_set_die_t *die,
    size_t *face_index)
{
    if (die == NULL || die->face_count == 0) {
        return NULL;
    }

    size_t index =
        (size_t)random_below(
            (uint32_t)die->face_count);

    if (face_index != NULL) {
        *face_index = index;
    }

    return &die->faces[index];
}

bool dice_custom_roll(
    const dice_set_definition_t *set,
    const dice_custom_pool_t *pool,
    dice_custom_roll_result_t *result)
{
    if (set == NULL ||
        pool == NULL ||
        result == NULL ||
        set->built_in ||
        set->die_count == 0) {
        return false;
    }

    memset(result, 0, sizeof(*result));

    for (size_t die_index = 0;
         die_index < set->die_count;
         ++die_index) {
        const dice_set_die_t *die =
            &set->dice[die_index];

        for (uint16_t instance = 0;
             instance < pool->quantity[die_index];
             ++instance) {
            size_t face_index = 0;
            const dice_set_face_t *face =
                dice_custom_random_face(
                    die,
                    &face_index);

            if (face == NULL) {
                continue;
            }

            ++result->total_count;

            if (face->has_numeric_value) {
                result->numeric_total +=
                    face->numeric_value;
            }

            if (result->count <
                DICE_CUSTOM_MAX_TOTAL) {
                dice_custom_result_die_t *output =
                    &result->dice[result->count++];

                output->die = die;
                output->face = face;
                output->die_index = die_index;
                output->face_index = face_index;
            }
        }
    }

    return result->total_count > 0;
}
