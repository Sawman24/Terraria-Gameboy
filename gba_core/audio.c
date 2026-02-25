#include "audio.h"

#define REG_SOUNDCNT_L  (*(volatile uint16_t*)0x04000080)
#define REG_SOUNDCNT_H  (*(volatile uint16_t*)0x04000082)
#define REG_SOUNDCNT_X  (*(volatile uint16_t*)0x04000084)
#define REG_SOUND1CNT_L (*(volatile uint16_t*)0x04000060)
#define REG_SOUND1CNT_H (*(volatile uint16_t*)0x04000062)
#define REG_SOUND1CNT_X (*(volatile uint16_t*)0x04000064)
#define REG_SOUND2CNT_L (*(volatile uint16_t*)0x04000068)
#define REG_SOUND2CNT_H (*(volatile uint16_t*)0x0400006C)

// Direct Sound & Timers
#define REG_TM0CNT_L    (*(volatile uint16_t*)0x04000100)
#define REG_TM0CNT_H    (*(volatile uint16_t*)0x04000102)
#define REG_DMA1SAD     (*(volatile uint32_t*)0x040000BC)
#define REG_DMA1DAD     (*(volatile uint32_t*)0x040000C0)
#define REG_DMA1CNT     (*(volatile uint32_t*)0x040000C4)
#define REG_FIFO_A      (*(volatile uint32_t*)0x040000A0)

static const NoteEvent* current_song = 0;
static int current_num_events = 0;
static int seq_index = 0;
static int seq_wait = 0;
static int ch1_dur = 0;
static int ch2_dur = 0;

static const signed char* sfx_ptr = 0;
static int sfx_remaining = 0;

void audio_init(void) {
    // 1. Master Enable
    REG_SOUNDCNT_X = 0x0080;
    
    // 2. PSG / Direct Sound Routing
    REG_SOUNDCNT_L = 0xFF77; // PSG channels to both L+R
    
    // REG_SOUNDCNT_H:
    // Bits 0-1: PSG Volume (10b = 100%)
    // Bit 2: DS A Volume (1b = 100%)
    // Bits 8-9: DS A Enable L+R (11b)
    // Bit 10: DS A Timer (0 = Timer 0)
    // Bit 11: DS A FIFO Reset
    REG_SOUNDCNT_H = 0x0B06; // Trigger Reset
    REG_SOUNDCNT_H = 0x0306; // Release Reset, DS A L+R, 100% Vol, T0
    
    // 3. Timer 0 for 11025Hz
    // GBA Clock is 16.78MHz. Timer ticks = Frequency / SampleRate
    // 16777216 / 11025 = 1521.74...
    REG_TM0CNT_L = 65536 - 1522;
    REG_TM0CNT_H = 0x0080; // Enable timer
}

static void psg_play_note(int channel, int freq_reg, int volume, int duty) {
    unsigned short env = (volume << 12);
    unsigned short duty_val = (duty << 6);
    unsigned short freq = freq_reg | 0x8000;
    
    if (channel == 0) {
        REG_SOUND1CNT_L = 0; // No sweep
        REG_SOUND1CNT_H = duty_val | env;
        REG_SOUND1CNT_X = freq;
    } else {
        REG_SOUND2CNT_L = duty_val | env;
        REG_SOUND2CNT_H = freq;
    }
}

static void psg_stop_channel(int channel) {
    if (channel == 0) {
        REG_SOUND1CNT_H = 0;
        REG_SOUND1CNT_X = 0x8000;
    } else {
        REG_SOUND2CNT_L = 0;
        REG_SOUND2CNT_H = 0x8000;
    }
}

void audio_play_song(const NoteEvent* notes, int num_events) {
    current_song = notes;
    current_num_events = num_events;
    seq_index = 0;
    seq_wait = notes[0].wait;
    ch1_dur = 0;
    ch2_dur = 0;
}

void audio_play_sfx(const signed char* data, int length) {
    sfx_ptr = data;
    sfx_remaining = length;
    
    // Stop any existing DMA
    REG_DMA1CNT = 0;
    
    // Set up DMA1 for Direct Sound A
    REG_DMA1SAD = (uint32_t)data;
    REG_DMA1DAD = (uint32_t)&REG_FIFO_A;
    // 0xB6400000:
    // Bit 31: Enable
    // Bits 28-29: Start Mode (Special/FIFO)
    // Bit 26: 32-bit
    // Bit 25: Repeat
    // Bits 21-22: Dest Control (2 = Fixed)
    REG_DMA1CNT = 0xB6400000;
}

void audio_update(void) {
    // 1. Update SFX
    if (sfx_remaining > 0) {
        sfx_remaining -= (11025 / 60); // Approx samples per frame
        if (sfx_remaining <= 0) {
            REG_DMA1CNT = 0; // Stop DMA
            sfx_remaining = 0;
        }
    }

    // 2. Update PSG Music
    if (!current_song) return;

    while (seq_index < current_num_events) {
        if (seq_wait > 0) {
            seq_wait--;
            break;
        }
        const NoteEvent* e = &current_song[seq_index];
        psg_play_note(e->channel, e->freq, e->volume, 1);
        if (e->channel == 0) ch1_dur = e->duration;
        else ch2_dur = e->duration;
        
        seq_index++;
        if (seq_index < current_num_events)
            seq_wait = current_song[seq_index].wait;
    }

    if (ch1_dur > 0) { ch1_dur--; if (ch1_dur == 0) psg_stop_channel(0); }
    if (ch2_dur > 0) { ch2_dur--; if (ch2_dur == 0) psg_stop_channel(1); }

    if (seq_index >= current_num_events && ch1_dur == 0 && ch2_dur == 0) {
        audio_play_song(current_song, current_num_events); // Loop
    }
}

void audio_stop(void) {
    current_song = 0;
    psg_stop_channel(0);
    psg_stop_channel(1);
    REG_DMA1CNT = 0;
    sfx_remaining = 0;
}
