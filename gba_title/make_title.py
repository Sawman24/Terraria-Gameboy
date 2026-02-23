"""
Composite the title screen and convert to GBA Mode 3 format (RGB555).
Output: title_screen.h with raw pixel data.
"""
from PIL import Image

# Load assets
bg = Image.open(r"c:\Users\sawye\Desktop\Terraria Beta\GBA_Assets\Misc\Main_Menu_BG.png").convert("RGBA")
logo0 = Image.open(r"c:\Users\sawye\Desktop\Terraria Beta\GBA_Assets\Misc\Logo_Part0.png").convert("RGBA")
logo1 = Image.open(r"c:\Users\sawye\Desktop\Terraria Beta\GBA_Assets\Misc\Logo_Part1.png").convert("RGBA")
logo2 = Image.open(r"c:\Users\sawye\Desktop\Terraria Beta\GBA_Assets\Misc\Logo_Part2.png").convert("RGBA")

# GBA screen
SCREEN_W, SCREEN_H = 240, 160

# Logo is 194px wide (3 parts of 64px, last one mostly empty)
logo_total_w = 64 * 3  # 192px
logo_h = 56

# Center the logo on screen
logo_x = (SCREEN_W - logo_total_w) // 2
logo_y = (SCREEN_H - logo_h) // 2 - 10  # slightly above center

# Composite
screen = bg.copy()
screen.paste(logo0, (logo_x, logo_y), logo0)
screen.paste(logo1, (logo_x + 64, logo_y), logo1)
screen.paste(logo2, (logo_x + 128, logo_y), logo2)

# Save preview
screen.save(r"c:\Users\sawye\Desktop\Terraria Beta\gba_title\title_preview.png")
print("Saved title_preview.png")

# Convert to GBA RGB555 format
# GBA: 15-bit color, bit layout: 0BBBBBGGGGGRRRRR
pixels = []
for y in range(SCREEN_H):
    for x in range(SCREEN_W):
        r, g, b, a = screen.getpixel((x, y))
        # Convert 8-bit to 5-bit
        r5 = r >> 3
        g5 = g >> 3
        b5 = b >> 3
        rgb555 = (b5 << 10) | (g5 << 5) | r5
        pixels.append(rgb555)

# Write C header
with open(r"c:\Users\sawye\Desktop\Terraria Beta\gba_title\title_screen.h", "w") as f:
    f.write("// Auto-generated title screen data (240x160, RGB555)\n")
    f.write(f"// Total size: {len(pixels) * 2} bytes\n\n")
    f.write("#ifndef TITLE_SCREEN_H\n")
    f.write("#define TITLE_SCREEN_H\n\n")
    f.write(f"#define TITLE_WIDTH {SCREEN_W}\n")
    f.write(f"#define TITLE_HEIGHT {SCREEN_H}\n\n")
    f.write(f"const unsigned short title_screen[{len(pixels)}] = {{\n")
    
    for i in range(0, len(pixels), 16):
        chunk = pixels[i:i+16]
        line = ", ".join(f"0x{p:04X}" for p in chunk)
        f.write(f"    {line},\n")
    
    f.write("};\n\n")
    f.write("#endif\n")

print(f"Saved title_screen.h ({len(pixels)} pixels, {len(pixels)*2} bytes)")
