import os
from PIL import Image

in_dir = r"c:\Users\sawye\Desktop\Terraria Beta\GBA_Assets\Player"
out_h = r"c:\Users\sawye\Desktop\Terraria Beta\gba_core\player_gfx.h"

# Components that make up a player (bottom to top)
LAYERS_STATIC = [
    ("Player_Head.png", (255, 200, 150)),      # tinted skin
    ("Player_Eye_Whites.png", (255, 255, 255)),
    ("Player_Eyes.png", (0, 0, 0)),
    ("Player_Hair.png", (130, 70, 30)),        # brown hair
]
LAYERS_ANIM = [
    ("Player_Undershirt.png", (255, 255, 255)),
    ("Player_Shirt.png", (200, 50, 50)),       # red shirt
    ("Player_Hands.png", (255, 200, 150)),
    ("Player_Pants.png", (50, 50, 200)),       # blue pants
    ("Player_Shoes.png", (100, 100, 100)),
]

colors = [(0, 0, 0)] # Transparent
color_map = {}

def apply_tint(crop, tint):
    colored = Image.new("RGBA", crop.size)
    for y in range(crop.height):
        for x in range(crop.width):
            r,g,b,a = crop.getpixel((x,y))
            if a > 0:
                nr = int(r * tint[0] / 255)
                ng = int(g * tint[1] / 255)
                nb = int(b * tint[2] / 255)
                colored.putpixel((x,y), (nr, ng, nb, a))
    return colored

frames_to_extract = [0, 1, 2] # Idle, Walk1, Walk2/Jump
all_tiles_8bpp = []

for frame_idx in frames_to_extract:
    # 32x32 container for the player
    img = Image.new("RGBA", (32, 32), (0,0,0,0))
    offset_x = 8 # center the 17px wide player
    offset_y = 6 # lower the 25px tall player near the bottom (32px full height)

    for name, tint in LAYERS_STATIC:
        path = os.path.join(in_dir, name)
        if os.path.exists(path):
            layer = Image.open(path).convert("RGBA")
            # For static, just use the top-left 17x25
            crop = apply_tint(layer.crop((0, 0, 17, 25)), tint)
            img.alpha_composite(crop, (offset_x, offset_y))

    for name, tint in LAYERS_ANIM:
        path = os.path.join(in_dir, name)
        if os.path.exists(path):
            layer = Image.open(path).convert("RGBA")
            # Specific layer height (might have 3 frames, 6 frames, etc)
            frames = layer.height // 25
            # So if pant has 3 frames, frame 2 logic works.
            f = frame_idx % frames
            crop = apply_tint(layer.crop((0, f*25, 17, (f+1)*25)), tint)
            img.alpha_composite(crop, (offset_x, offset_y))
            
    # Now tile it (32x32 divides into 4x4 tiles = 16 tiles)
    for ty in range(4):
        for tx in range(4):
            tile = []
            for y in range(8):
                for x in range(8):
                    px = tx*8 + x
                    py = ty*8 + y
                    r,g,b,a = img.getpixel((px, py))
                    if a < 128:
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

# Dump to header
print("Generating player_gfx.h")
with open(out_h, "w") as f:
    f.write("#ifndef PLAYER_GFX_H\n#define PLAYER_GFX_H\n\n")
    
    pal_data = []
    for r, g, b in colors:
        rgb555 = ((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3)
        pal_data.append(rgb555)

    while len(pal_data) < 256:
        pal_data.append(0)
        
    f.write(f"const unsigned short player_palette[256] = {{\n")
    for i in range(0, 256, 16):
        chunk = pal_data[i:i+16]
        f.write("    " + ", ".join(f"0x{c:04X}" for c in chunk) + ",\n")
    f.write("};\n\n")
    
    f.write(f"const unsigned char player_graphics[{len(all_tiles_8bpp)*64}] __attribute__((aligned(4))) = {{\n")
    for ti, tile in enumerate(all_tiles_8bpp):
        f.write(f"    // Tile {ti}\n")
        f.write("    ")
        for i in range(0, 64, 16):
            chunk = tile[i:i+16]
            f.write(", ".join(f"0x{b:02X}" for b in chunk) + ", ")
        f.write("\n")
    f.write("};\n\n")
    
    f.write("#endif\n")
    
print(f"Done! Extracted {len(frames_to_extract)} frames ({len(all_tiles_8bpp)} tiles) using {len(colors)} colors.")
