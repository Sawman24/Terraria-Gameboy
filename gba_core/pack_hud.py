import os
from PIL import Image

in_path = r"c:\Users\sawye\Desktop\Terraria Beta\GBA_Assets\Misc\Heart.png"
out_h = r"c:\Users\sawye\Desktop\Terraria Beta\gba_core\hud_gfx.h"

colors = [(0, 0, 0)] # Transparent
color_map = {}

img = Image.new("RGBA", (16, 16), (0,0,0,0))
if os.path.exists(in_path):
    heart = Image.open(in_path).convert("RGBA")
    # center the 11x11 heart into the 16x16 block
    img.alpha_composite(heart, (2, 2))

all_tiles_8bpp = []
# Tiled 2x2 blocks for 16x16
for ty in range(2):
    for tx in range(2):
        tile = []
        for y in range(8):
            for x in range(8):
                px = tx*8 + x
                py = ty*8 + y
                r,g,b,a = img.getpixel((px, py))
                # Terraria heart transparent might be magenta or alpha
                if a < 128 or (r == 247 and b == 249):
                    tile.append(0)
                else:
                    rgb = (r, g, b)
                    if rgb not in color_map:
                        if len(colors) < 256:
                            color_map[rgb] = len(colors)
                            colors.append(rgb)
                        else:
                            color_map[rgb] = 1
                    tile.append(color_map[rgb])
        all_tiles_8bpp.append(tile)

print("Generating hud_gfx.h")
with open(out_h, "w") as f:
    f.write("#ifndef HUD_GFX_H\n#define HUD_GFX_H\n\n")
    
    pal_data = []
    for r, g, b in colors:
        rgb555 = ((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3)
        pal_data.append(rgb555)

    while len(pal_data) < 256:
        pal_data.append(0)
        
    f.write(f"const unsigned short hud_palette[256] = {{\n")
    for i in range(0, 256, 16):
        chunk = pal_data[i:i+16]
        f.write("    " + ", ".join(f"0x{c:04X}" for c in chunk) + ",\n")
    f.write("};\n\n")
    
    f.write(f"const unsigned char hud_graphics[{len(all_tiles_8bpp)*64}] __attribute__((aligned(4))) = {{\n")
    for ti, tile in enumerate(all_tiles_8bpp):
        f.write(f"    // Tile {ti}\n")
        f.write("    ")
        for i in range(0, 64, 16):
            chunk = tile[i:i+16]
            f.write(", ".join(f"0x{b:02X}" for b in chunk) + ", ")
        f.write("\n")
    f.write("};\n\n")
    
    f.write("#endif\n")
    
print(f"Done! Extracted {len(all_tiles_8bpp)} tiles using {len(colors)} colors.")
