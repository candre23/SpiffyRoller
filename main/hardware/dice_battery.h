#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef void (*dice_battery_update_callback_t)(
    bool available,
    uint16_t voltage_mv,
    uint8_t percent,
    bool usb_power_present,
    void *context);

bool dice_battery_start(
    dice_battery_update_callback_t callback,
    void *context);
