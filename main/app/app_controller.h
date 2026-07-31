#pragma once
#include <stdbool.h>
bool app_controller_start(void);
void app_controller_notify_shake(void *context);
void app_controller_notify_boot(bool long_press);
