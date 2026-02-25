#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>
#include "audio_types.h"

void audio_init(void);
void audio_play_song(const NoteEvent* notes, int num_events);
void audio_play_sfx(const signed char* data, int length);
void audio_update(void);
void audio_stop(void);

#endif
