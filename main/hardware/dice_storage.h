#pragma once

#include <stdbool.h>
#include <stddef.h>

#define DICE_STORAGE_MOUNT_POINT "/storage"
#define DICE_ROLL_WAV_PATH \
    DICE_STORAGE_MOUNT_POINT "/audio/dice_roll.wav"

bool dice_storage_mount(void);
bool dice_storage_is_mounted(void);
bool dice_storage_get_usage(size_t *total_bytes, size_t *used_bytes);
bool dice_storage_unmount(void);
