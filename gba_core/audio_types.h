#ifndef AUDIO_TYPES_H
#define AUDIO_TYPES_H

typedef struct {
    unsigned short wait;     // frames to wait before this event
    unsigned short freq;     // GBA frequency register value (0-2047)
    unsigned short duration; // how long to play (frames)
    unsigned char  volume;   // 0-15
    unsigned char  channel;  // 0=ch1, 1=ch2
} NoteEvent;

#endif
