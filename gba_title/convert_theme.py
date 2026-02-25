import mido
import os

# Paths relative to this script
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
# Note: based on the file structure, gba_title and GBA_Assets are siblings in the root.
MIDI_PATH = os.path.join(BASE_DIR, "..", "GBA_Assets", "Sounds", "Terraria_theme.mid")
OUT_HEADER = os.path.join(BASE_DIR, "..", "gba_core", "theme_data.h")

def midi_note_to_gba_freq(note):
    freq = 440.0 * (2.0 ** ((note - 69) / 12.0))
    reg = int(2048 - 131072 / freq)
    if reg < 0: reg = 0
    if reg > 2047: reg = 2047
    return reg

def convert_midi(midi_file, header_file):
    if not os.path.exists(midi_file):
        print(f"Error: {midi_file} not found")
        return

    mid = mido.MidiFile(midi_file)
    tempo = 500000  # Default 120 BPM
    for track in mid.tracks:
        for msg in track:
            if msg.type == 'set_tempo':
                tempo = msg.tempo
                break

    bpm = 60000000 / tempo
    ticks_per_frame = mid.ticks_per_beat * bpm / 3600.0
    
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
    
    active = {}
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
    if not notes:
        print("No notes found!")
        return

    max_frame = max(n[0] + n[2] for n in notes)
    sequence = []
    prev_frame = 0
    for start_frame, note, dur_frames, vel, track in notes:
        wait = start_frame - prev_frame
        gba_freq = midi_note_to_gba_freq(note)
        gba_vol = min(15, max(1, vel * 15 // 127))
        ch = min(track, 1) # Only 2 PSG square channels
        sequence.append((wait, gba_freq, dur_frames, gba_vol, ch))
        prev_frame = start_frame

    with open(header_file, 'w') as f:
        f.write("#ifndef THEME_DATA_H\n#define THEME_DATA_H\n\n")
        f.write("#include \"audio_types.h\"\n\n")
        f.write(f"#define THEME_NUM_EVENTS {len(sequence)}\n")
        f.write(f"const NoteEvent theme_notes[{len(sequence)}] = {{\n")
        for wait, freq, dur, vol, ch in sequence:
            f.write(f"    {{{wait}, {freq}, {dur}, {vol}, {ch}}},\n")
        f.write("};\n\n#endif\n")
    print(f"Generated {header_file}")

if __name__ == "__main__":
    convert_midi(MIDI_PATH, OUT_HEADER)
