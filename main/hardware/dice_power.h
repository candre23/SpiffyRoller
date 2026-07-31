#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef void (*dice_power_shutdown_callback_t)(
    const char *reason,
    void *context);

bool dice_power_start(
    dice_power_shutdown_callback_t shutdown_callback,
    void *context);

void dice_power_note_activity(void);
void dice_power_high_performance_acquire(void);
void dice_power_high_performance_release(void);

void dice_power_update_battery(
    bool available,
    uint16_t voltage_mv,
    bool usb_power_present);
