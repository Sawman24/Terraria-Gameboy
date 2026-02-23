"""
Convert Terraria MIDI theme to GBA PSG note sequence.
Output: compact note data for hardware square wave channels.
Format: array of (duration_frames, channel, note, volume) entries.
"""
import mido

MIDI_PATH = r"c:\Users\sawye\Desktop\Terraria Beta\GBA_Assets\Sounds\Terraria_theme.mid"
OUT_HEADER = r"c:\Users\sawye\Desktop\Terraria Beta\gba_title\theme_data.h"

# MIDI note to GBA frequency register value
# GBA square wave freq: rate = 131072 / (2048 - reg_value) Hz
# So reg_value = 2048 - 131072 / freq
def midi_note_to_gba_freq(note):
    freq = 440.0 * (2.0 ** ((note - 69) / 12.0))
    reg = int(2048 - 131072 / freq)
    if reg < 0: reg = 0
    if reg > 2047: reg = 2047
    return reg

def midi_note_name(note):
    names = ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B']
    return f"{names[note%12]}{note//12-1}"

if __name__ == "__main__":
    print("=== Converting MIDI to GBA PSG Sequence ===\n")
    
    mid = mido.MidiFile(MIDI_PATH)
    print(f"Ticks per beat: {mid.ticks_per_beat}")
    print(f"Tracks: {len(mid.tracks)}")
    
    # Collect events per track
    tempo = 500000  # 120 BPM default
    
    for i, track in enumerate(mid.tracks):
        print(f"\nTrack {i}: {track.name if track.name else '(unnamed)'}")
        abs_tick = 0
        for msg in track:
            abs_tick += msg.time
            if msg.type == 'set_tempo':
                tempo = msg.tempo
                print(f"  Tempo: {tempo} us/beat = {60000000/tempo:.0f} BPM")
            elif msg.type in ('note_on', 'note_off'):
                if msg.type == 'note_on' and msg.velocity > 0:
                    print(f"  tick {abs_tick:5d}: ON  {midi_note_name(msg.note):4s} (note={msg.note}, vel={msg.velocity})")
                else:
                    print(f"  tick {abs_tick:5d}: OFF {midi_note_name(msg.note):4s}")
    
    # Build a sequence of events sorted by time
    # We'll use 2 PSG channels: ch1 for melody, ch2 for harmony
    bpm = 60000000.0 / tempo
    ticks_per_frame = mid.ticks_per_beat * bpm / 60.0 / 60.0  # ticks per GBA frame (60fps)
    print(f"\nBPM: {bpm:.0f}, ticks/frame: {ticks_per_frame:.2f}")
    
    # Collect all note on/off events with absolute tick times
    events = []
    for track_idx, track in enumerate(mid.tracks):
        abs_tick = 0
        for msg in track:
            abs_tick += msg.time
            if msg.type == 'note_on' and msg.velocity > 0:
                events.append((abs_tick, 'on', msg.note, msg.velocity, track_idx))
            elif msg.type == 'note_off' or (msg.type == 'note_on' and msg.velocity == 0):
                events.append((abs_tick, 'off', msg.note, 0, track_idx))
    
    events.sort(key=lambda e: (e[0], 0 if e[1]=='off' else 1))
    
    # Build note sequence: list of (frame_time, note, duration_frames)
    active = {}  # note -> (start_tick, track)
    notes = []
    
    for tick, etype, note, vel, track in events:
        if etype == 'on':
            active[note] = (tick, track, vel)
        elif etype == 'off' and note in active:
            start_tick, start_track, start_vel = active.pop(note)
            dur_ticks = tick - start_tick
            start_frame = int(start_tick / ticks_per_frame)
            dur_frames = max(1, int(dur_ticks / ticks_per_frame))
            notes.append((start_frame, note, dur_frames, start_vel, start_track))
    
    notes.sort(key=lambda n: (n[0], n[1]))
    
    print(f"\nTotal notes: {len(notes)}")
    
    # Find total duration in frames
    max_frame = max(n[0] + n[2] for n in notes)
    print(f"Total duration: {max_frame} frames ({max_frame/60:.1f}s)")
    
    # Build frame-based sequence
    # Format: each entry = {wait_frames, gba_freq, duration_frames, channel}
    # Channel 0 = PSG ch1, Channel 1 = PSG ch2
    # We assign notes to channels round-robin or by track
    
    sequence = []
    prev_frame = 0
    
    for start_frame, note, dur_frames, vel, track in notes:
        wait = start_frame - prev_frame
        gba_freq = midi_note_to_gba_freq(note)
        # Volume: 0-15 for GBA envelope
        gba_vol = min(15, max(1, vel * 15 // 127))
        # Channel: use track index (0 or 1)
        ch = min(track, 1)
        
        sequence.append((wait, gba_freq, dur_frames, gba_vol, ch))
        prev_frame = start_frame
    
    print(f"Sequence entries: {len(sequence)}")
    data_size = len(sequence) * 5  # 5 bytes per entry (approx)
    print(f"Estimated data size: {data_size} bytes")
    
    # Write C header
    with open(OUT_HEADER, 'w') as f:
        f.write("// Auto-generated Terraria theme - PSG note sequence\n")
        f.write(f"// {len(sequence)} events, ~{data_size} bytes\n")
        f.write(f"// Duration: {max_frame} frames ({max_frame/60:.1f}s)\n\n")
        f.write("#ifndef THEME_DATA_H\n#define THEME_DATA_H\n\n")
        f.write(f"#define THEME_NUM_EVENTS {len(sequence)}\n")
        f.write(f"#define THEME_TOTAL_FRAMES {max_frame}\n\n")
        
        f.write("// Each event: {wait_frames, freq_reg, duration_frames, volume, channel}\n")
        f.write("typedef struct {\n")
        f.write("    unsigned short wait;     // frames to wait before this event\n")
        f.write("    unsigned short freq;     // GBA frequency register value (0-2047)\n")
        f.write("    unsigned short duration; // how long to play (frames)\n")
        f.write("    unsigned char  volume;   // 0-15\n")
        f.write("    unsigned char  channel;  // 0=ch1, 1=ch2\n")
        f.write("} NoteEvent;\n\n")
        
        f.write(f"const NoteEvent theme_notes[{len(sequence)}] = {{\n")
        for i, (wait, freq, dur, vol, ch) in enumerate(sequence):
            f.write(f"    {{{wait:3d}, {freq:4d}, {dur:3d}, {vol:2d}, {ch}}},")
            # Add comment with note name
            # Reverse: freq_hz = 131072 / (2048 - freq)
            if freq < 2048:
                fhz = 131072.0 / (2048 - freq)
                f.write(f" // {fhz:.0f}Hz")
            f.write("\n")
        f.write("};\n\n")
        
        f.write("#endif\n")
    
    print(f"\nWrote {OUT_HEADER}")
    print(f"Final data size: {len(sequence) * 8} bytes ({len(sequence) * 8 / 1024:.1f} KB)")
