"""
Generate GBA title screen data for Mode 3 (bitmap) approach.
- Background as full 240x160 RGB555 bitmap
- Logo parts as individual RGB555 bitmaps with transparency
- Embedded bitmap font
"""
from PIL import Image
import os

OUT_DIR = r"c:\Users\sawye\Desktop\Terraria Beta\gba_title"

def rgb555(r, g, b):
    return ((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3)

def write_array_u16(f, name, data, comment=""):
    if comment:
        f.write(f"// {comment}\n")
    f.write(f"const unsigned short {name}[{len(data)}] = {{\n")
    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        f.write("    " + ", ".join(f"0x{v:04X}" for v in chunk) + ",\n")
    f.write("};\n\n")

def write_array_u8(f, name, data, comment=""):
    if comment:
        f.write(f"// {comment}\n")
    f.write(f"const unsigned char {name}[{len(data)}] = {{\n")
    for i in range(0, len(data), 20):
        chunk = data[i:i+20]
        f.write("    " + ", ".join(f"0x{v:02X}" for v in chunk) + ",\n")
    f.write("};\n\n")

if __name__ == "__main__":
    print("=== Generating Mode 3 GBA Data ===\n")
    
    # --- Background (full 240x160 bitmap) ---
    print("Creating background...")
    bg_tile = Image.open(r"c:\Users\sawye\Desktop\Terraria Beta\GBA_Assets\Backgrounds\Background_2.png").convert("RGB")
    tw, th = bg_tile.size
    
    bg_pixels = []
    for y in range(160):
        for x in range(240):
            r, g, b = bg_tile.getpixel((x % tw, y % th))
            bg_pixels.append(rgb555(r, g, b))
    print(f"  Background: 240x160 = {len(bg_pixels)} pixels")
    
    # --- Logo parts (with alpha mask) ---
    print("Creating logo sprites...")
    logo_parts_data = []
    logo_parts_alpha = []
    logo_dims = []
    
    for i in range(3):
        path = os.path.join(OUT_DIR, "..", "GBA_Assets", "Misc", f"Logo_Part{i}.png")
        img = Image.open(path).convert("RGBA")
        w, h = img.size
        logo_dims.append((w, h))
        
        pixels = []
        alpha = []
        for y in range(h):
            for x in range(w):
                r, g, b, a = img.getpixel((x, y))
                pixels.append(rgb555(r, g, b))
                alpha.append(1 if a > 128 else 0)
        
        logo_parts_data.append(pixels)
        logo_parts_alpha.append(alpha)
        print(f"  Logo part {i}: {w}x{h}")
    
    # --- Write header ---
    print("Writing game_data.h...")
    with open(os.path.join(OUT_DIR, "game_data.h"), "w") as f:
        f.write("// Auto-generated GBA title screen data (Mode 3)\n")
        f.write("#ifndef GAME_DATA_H\n#define GAME_DATA_H\n\n")
        
        write_array_u16(f, "bg_bitmap", bg_pixels, "Background 240x160 RGB555")
        
        for i in range(3):
            w, h = logo_dims[i]
            f.write(f"#define LOGO_{i}_W {w}\n")
            f.write(f"#define LOGO_{i}_H {h}\n")
            write_array_u16(f, f"logo_{i}_pixels", logo_parts_data[i], f"Logo part {i} ({w}x{h}) RGB555")
            write_array_u8(f, f"logo_{i}_alpha", logo_parts_alpha[i], f"Logo part {i} alpha mask")
        
        # Splash texts
        f.write("// Splash texts\n")
        f.write("const char* const splash_texts[] = {\n")
        splashes = [
            "Also try Minecraft!",
            "Now on GBA!",
            "Dig! Fight! Build!",
            "Sand is overpowered",
            "Guide was slain...",
            "Beta version!",
            "Not the bees!",
            "Press START!",
            "A goblin army!",
            "Blood Moon rising",
            "The jungle grows",
            "Cthulhu watches",
        ]
        for s in splashes:
            f.write(f'    "{s}",\n')
        f.write("};\n")
        f.write(f"#define NUM_SPLASH {len(splashes)}\n\n")
        
        f.write("#endif\n")
    
    print("Done!")
