#pragma once

#include <stdbool.h>

/*
 * Pulse the AMOLED controller's hardware reset line through the onboard
 * TCA9554 I/O expander. This is needed after warm resets because the external
 * display controller remains powered and does not necessarily reset with the
 * ESP32-S3 CPU.
 */
bool dice_display_hardware_reset(void);
