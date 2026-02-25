import wave
import numpy as np
import os
import struct

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
SOUNDS_DIR = os.path.join(BASE_DIR, "..", "GBA_Assets", "Sounds")
OUT_HEADER = os.path.join(BASE_DIR, "sfx_data.h")

TARGET_RATE = 11025

def pack_sfx(filename, name):
    path = os.path.join(SOUNDS_DIR, filename)
    if not os.path.exists(path):
        print(f"Warning: {path} not found")
        return None, 0

    with wave.open(path, 'rb') as f:
        channels = f.getnchannels()
        width = f.getsampwidth()
        rate = f.getframerate()
        n_frames = f.getnframes()
        data = f.readframes(n_frames)

    # Convert to numpy array
    if width == 1:
        samples = np.frombuffer(data, dtype=np.uint8).astype(np.float32) - 128
    elif width == 2:
        samples = np.frombuffer(data, dtype=np.int16).astype(np.float32)
    else:
        return None, 0

    # Mono conversion
    if channels > 1:
        samples = samples.reshape(-1, channels).mean(axis=1)

    # Downsample
    if rate != TARGET_RATE:
        input_bits = np.arange(len(samples))
        output_bits = np.linspace(0, len(samples)-1, int(len(samples) * TARGET_RATE / rate))
        samples = np.interp(output_bits, input_bits, samples)

    # Normalize and convert to 8-bit signed
    max_val = np.abs(samples).max()
    if max_val > 0:
        samples = (samples / max_val * 127).astype(np.int8)
    else:
        samples = samples.astype(np.int8)

    return samples.tobytes(), TARGET_RATE

def main():
    sfx_list = [
        ("Dig_0.wav", "sfx_dig0"),
        ("Dig_1.wav", "sfx_dig1"),
        ("Dig_2.wav", "sfx_dig2"),
        ("Player_Hit_0.wav", "sfx_hurt"),
        ("Item_1.wav", "sfx_swing"),
        ("Grab.wav", "sfx_grab"),
        ("NPC_Hit_1.wav", "sfx_hit"),
    ]

    with open(OUT_HEADER, "w") as f:
        f.write("#ifndef SFX_DATA_H\n#define SFX_DATA_H\n\n")
        
        for filename, var_name in sfx_list:
            data, rate = pack_sfx(filename, var_name)
            if data:
                f.write(f"// {filename} at {rate}Hz\n")
                f.write(f"const int {var_name}_len = {len(data)};\n")
                f.write(f"const signed char {var_name}[] __attribute__((aligned(4))) = {{\n")
                for i in range(0, len(data), 16):
                    chunk = data[i:i+16]
                    f.write("    " + ", ".join(f"{b if b < 128 else b-256}" for b in chunk) + ",\n")
                f.write("};\n\n")
        
        f.write("#endif\n")
    print(f"Generated {OUT_HEADER}")

if __name__ == "__main__":
    main()
