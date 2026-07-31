#pragma once

#include <stdbool.h>

typedef void (*dice_imu_shake_callback_t)(void *context);
typedef void (*dice_imu_motion_callback_t)(void *context);

bool dice_imu_start(
    dice_imu_shake_callback_t shake_callback,
    dice_imu_motion_callback_t motion_callback,
    void *context);

void dice_imu_set_sensitivity(int level);
