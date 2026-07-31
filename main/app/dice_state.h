#pragma once

#include <stdbool.h>
#include "dice_model.h"
#include "dice_set_catalog.h"

typedef struct {
    dice_pool_t pool;
    bool sound_enabled;
    int brightness;
    int shake_level;
    int display_timeout_seconds;
    int selector_hide_ms;
    char active_set_id[DICE_SET_ID_MAX + 1];
} dice_saved_state_t;

bool dice_state_load(dice_saved_state_t *state);
bool dice_state_save(const dice_saved_state_t *state);
