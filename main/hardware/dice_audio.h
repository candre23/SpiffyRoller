#pragma once
#include <stdbool.h>
bool dice_audio_start(void);
bool dice_audio_play_roll(void);
bool dice_audio_play_tick(void);
void dice_audio_set_enabled(bool enabled);
bool dice_audio_is_enabled(void);
void dice_audio_stop(void);
