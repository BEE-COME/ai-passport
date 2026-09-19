#pragma once

#include <stdbool.h>
#include <stdint.h>

bool audio_player_start(void);
void audio_player_play(uint16_t word_index);
void audio_player_stop(void);
