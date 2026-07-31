#pragma once

#include <stdbool.h>

bool dice_transfer_initialize_flag_storage(void);
bool dice_transfer_was_requested(void);
bool dice_transfer_request_and_reboot(void);
void dice_transfer_run(void);
