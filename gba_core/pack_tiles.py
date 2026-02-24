import os
from PIL import Image

# The order of tiles we want to extract
TILE_DEFS = [
    # (name, file_name, px, py)
    ("Air", None, 0, 0),             # 0
    ("Dirt", r"Tiles\Tiles_0.png", 9, 9),   # 1  (sx=1, sy=1)
    ("Stone", r"Tiles\Tiles_1.png", 9, 9),  # 2  (sx=1, sy=1)
    ("Grass", r"Tiles\Tiles_2.png", 9, 0),  # 3  (sx=1, sy=0)
    
    # Trees have weird alignments in the spritesheet. 
    # Tile 0,0 is 7px wide starting at x=1. Let's crop it exactly at x=1, and it's 7px wide.
    # To center it in an 8x8 block, we'll actually just use the sprite from px=2 to get 6px wide.
    ("Wood", r"Tiles\Tiles_5.png", 1, 0),   # 4  (Tree trunk perfectly centered)
    
    ("Leaves", r"Misc\Tree_Tops.png", 9, 9), # 5 (sx=1, sy=1)
    ("Copper", r"Tiles\Tiles_7.png", 9, 9), # 6
    ("Iron", r"Tiles\Tiles_6.png", 9, 9),   # 7
    ("Planks", r"Tiles\Tiles_30.png", 9, 9), # 8
    ("Torch", r"Tiles\Tiles_4.png", 0, 0),  # 9
    ("Ash", r"Tiles\Tiles_57.png", 9, 9),   # 10
    ("GrassPlants", r"Tiles\Tiles_3.png", 0, 0), # 11
    ("Lava", None, 0, 0),                    # 12
    ("Furnace", r"Tiles\Tiles_17.png", 8, 0),    # 13
    ("Workbench", r"Tiles\Tiles_18.png", 1, 0),  # 14
    ("Chest", r"Tiles\Tiles_21.png", 8, 0),      # 15
    ("Mud", r"Tiles\Tiles_59.png", 9, 9),        # 16
    ("JungleGrass", r"Tiles\Tiles_60.png", 9, 0),   # 17
    ("Empty18", None, 0, 0),                      # 18
    ("Empty19", None, 0, 0),                      # 19
    ("Sapling", r"Tiles\Tiles_20.png", 0, 0),    # 20
]

IN_DIR = r"c:\Users\sawye\Desktop\Game Dev\Terraria-Gameboy\GBA_Assets"
OUT_HEADER = r"c:\Users\sawye\Desktop\Game Dev\Terraria-Gameboy\gba_core\tileset.h"

# 1. Collect all unique colors across these tiles for an 8bpp (256-color) palette
colors = [(132, 170, 248)] # Index 0 = Sky Blue (transparent in our tiles)
color_map = {}

tile_data_8bpp = []

for name, filename, px, py in TILE_DEFS:
    tile_indices = []
    if filename is None:
        # Air
        if name == "Black":
            tile_indices = [1] * 64
            if (0,0,0) not in color_map:
                color_map[(0,0,0)] = 1
                if len(colors) <= 1: colors.append((0,0,0))
                else: colors[1] = (0,0,0)
        else:
            tile_indices = [0] * 64
        tile_data_8bpp.append(tile_indices)
        continue
    
    path = os.path.join(IN_DIR, filename)
    if not os.path.exists(path):
        print(f"Skipping {filename} (not found, using Air block)")
        tile_data_8bpp.append([0] * 64)
        continue
        
    img = Image.open(path).convert("RGBA")
    
    tile_img = img.crop((px, py, px + 8, py + 8))
    
    # If it's one of our new furniture tiles, let's grab the 16x16 and downscale to 8x8 to make it look better
    if name in ["Furnace", "Workbench", "Chest"]:
        tile_img = img.crop((px, py, px + 16, py + 16)).resize((8, 8), resample=Image.LANCZOS)

    for y in range(8):
        for x in range(8):
            rgb_val = tile_img.getpixel((x, y))
            # Treat pure magenta as transparent/padding as well
            magenta = (rgb_val[0] == 247 and rgb_val[2] == 249) or (rgb_val[0]==255 and rgb_val[2]==255 and rgb_val[1]==0)
            
            if rgb_val[3] < 128 or magenta:
                tile_indices.append(0)
            else:
                rgb = (rgb_val[0], rgb_val[1], rgb_val[2])
                
                if rgb not in color_map:
                    if len(colors) < 256:
                        color_map[rgb] = len(colors)
                        colors.append(rgb)
                    else:
                        color_map[rgb] = 1
                tile_indices.append(color_map[rgb])
    tile_data_8bpp.append(tile_indices)

# Check if we have more variants to add, but these 8 tiles are enough for gen.

# --- Extract 3 Tree Tops (40x40 pixels = 5x5 tiles each) ---
tree_tops_path = os.path.join(IN_DIR, r"Misc\Tree_Tops.png")
if os.path.exists(tree_tops_path):
    tt_img = Image.open(tree_tops_path).convert("RGBA")
    # Tree Tops are at offsets: 0, 41, 82. We'll extract 40x40 (5x5 tiles)
    offsets = [0, 41, 82]
    for start_x in offsets:
        for ty in range(5):
            for tx in range(5):
                tile_indices = []
                for y in range(8):
                    for x in range(8):
                        px = start_x + tx*8 + x
                        py = ty*8 + y
                        # Bounds check
                        if px >= tt_img.width or py >= tt_img.height:
                            tile_indices.append(0)
                            continue
                            
                        rgb_val = tt_img.getpixel((px, py))
                        magenta = (rgb_val[0] == 247 and rgb_val[2] == 249) or (rgb_val[0]==255 and rgb_val[2]==255 and rgb_val[1]==0)
                        
                        if rgb_val[3] < 128 or magenta:
                            tile_indices.append(0)
                        else:
                            rgb = (rgb_val[0], rgb_val[1], rgb_val[2])
                            if rgb not in color_map:
                                if len(colors) < 256:
                                    color_map[rgb] = len(colors)
                                    colors.append(rgb)
                                else:
                                    color_map[rgb] = 1
                            tile_indices.append(color_map[rgb])
                tile_data_8bpp.append(tile_indices)

# --- Extract 6 Tree Branches (16x16 pixels = 2x2 tiles each) ---
branches_path = os.path.join(IN_DIR, r"Misc\Tree_Branches.png")
if os.path.exists(branches_path):
    br_img = Image.open(branches_path).convert("RGBA")
    # 3 rows of branches, each row has a Left and a Right branch.
    # Each cell is roughly 21x21. Total width 42, height 63.
    # Left Branch at x=0..20, Right Branch at x=21..41.
    for row in range(3):
        y0 = row * 21
        # Extract 2x2 tiles (16x16) for Left and Right
        # Offsets chosen to avoid the trunk in the middle (x=20)
        # Left: (0, 2), Right: (26, 2) relative to cell top
        for side_x in [0, 26]:
            for ty in range(2):
                for tx in range(2):
                    tile_indices = []
                    for y in range(8):
                        for x in range(8):
                            px = side_x + tx*8 + x
                            py = y0 + 2 + ty*8 + y
                            if px >= br_img.width or py >= br_img.height:
                                tile_indices.append(0)
                                continue
                            rgb_val = br_img.getpixel((px, py))
                            magenta = (rgb_val[0] == 247 and rgb_val[2] == 249) or (rgb_val[0]==255 and rgb_val[2]==255 and rgb_val[1]==0)
                            if rgb_val[3] < 128 or magenta:
                                tile_indices.append(0)
                            else:
                                rgb = (rgb_val[0], rgb_val[1], rgb_val[2])
                                if rgb not in color_map:
                                    if len(colors) < 256:
                                        color_map[rgb] = len(colors)
                                        colors.append(rgb)
                                    else:
                                        color_map[rgb] = 1
                                tile_indices.append(color_map[rgb])
                    tile_data_8bpp.append(tile_indices)

# --- Extract Backgrounds (48x48 = 6x6 tiles each) ---
backgrounds = [
    r"Backgrounds\Background_2.png",
    r"Backgrounds\Background_3.png"
]
for bg_rel in backgrounds:
    bg_p = os.path.join(IN_DIR, bg_rel)
    if os.path.exists(bg_p):
        bg_img = Image.open(bg_p).convert("RGBA")
        for ty in range(6):
            for tx in range(6):
                tile_indices = []
                for y in range(8):
                    for x in range(8):
                        px = tx*8 + x
                        py = ty*8 + y
                        if px >= bg_img.width or py >= bg_img.height:
                            tile_indices.append(0)
                            continue
                        rgb_val = bg_img.getpixel((px, py))
                        if rgb_val[3] < 128:
                            tile_indices.append(0)
                        else:
                            rgb = (rgb_val[0], rgb_val[1], rgb_val[2])
                            if rgb not in color_map:
                                if len(colors) < 256:
                                    color_map[rgb] = len(colors)
                                    colors.append(rgb)
                                else:
                                    color_map[rgb] = 1
                            tile_indices.append(color_map[rgb])
                tile_data_8bpp.append(tile_indices)
    else:
        for _ in range(36):
            tile_data_8bpp.append([0]*64)

# --- Extract Clouds (50% size) ---
clouds_info = [
    (r"Misc\Cloud_0.png", (64, 26), 8, 4),
    (r"Misc\Cloud_1.png", (44, 16), 6, 2),
    (r"Misc\Cloud_2.png", (40, 20), 5, 3),
]
for rel_p, size, tw, th in clouds_info:
    cl_p = os.path.join(IN_DIR, rel_p)
    if os.path.exists(cl_p):
        cl_raw = Image.open(cl_p).convert("RGBA")
        cl_img = cl_raw.resize(size, resample=Image.LANCZOS)
        for ty in range(th):
            for tx in range(tw):
                tile_indices = []
                for y in range(8):
                    for x in range(8):
                        px = tx*8 + x
                        py = ty*8 + y
                        if px >= cl_img.width or py >= cl_img.height:
                            tile_indices.append(0)
                        else:
                            rgb_val = cl_img.getpixel((px, py))
                            if rgb_val[3] < 64: # Transparent enough
                                tile_indices.append(0)
                            else:
                                rgb = (rgb_val[0], rgb_val[1], rgb_val[2])
                                if rgb not in color_map:
                                    if len(colors) < 256:
                                        color_map[rgb] = len(colors)
                                        colors.append(rgb)
                                    else:
                                        color_map[rgb] = 1
                                tile_indices.append(color_map[rgb])
                tile_data_8bpp.append(tile_indices)
    else:
        for _ in range(tw*th): tile_data_8bpp.append([0]*64)

# Convert palette to GBA RGB555
pal_data = []
for r, g, b in colors:
    rgb555 = ((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3)
    pal_data.append(rgb555)

while len(pal_data) < 256:
    pal_data.append(0)

# Write output header
with open(OUT_HEADER, "w") as f:
    f.write("// Auto-generated GBA Tile Set for World Engine\n")
    f.write("#ifndef TILESET_H\n#define TILESET_H\n\n")
    
    f.write(f"const unsigned short tileset_palette[{len(pal_data)}] = {{\n")
    for i in range(0, len(pal_data), 16):
        chunk = pal_data[i:i+16]
        f.write("    " + ", ".join(f"0x{c:04X}" for c in chunk) + ",\n")
    f.write("};\n\n")
    
    total_bytes = len(tile_data_8bpp) * 64
    f.write(f"const unsigned char tileset_graphics[{total_bytes}] __attribute__((aligned(4))) = {{\n")
    for ti, tile in enumerate(tile_data_8bpp):
        name = TILE_DEFS[ti][0] if ti < len(TILE_DEFS) else f"TreeTop_Tile_{ti - len(TILE_DEFS)}"
        f.write(f"    // Tile {ti} - {name}\n")
        f.write("    ")
        for i in range(0, 64, 16):
            chunk = tile[i:i+16]
            f.write(", ".join(f"0x{b:02X}" for b in chunk) + ", ")
        f.write("\n")
    f.write("};\n\n")
    
    f.write(f"#define NUM_TILES {len(tile_data_8bpp)}\n")
    f.write("#endif\n")

print(f"Generated tileset with {len(tile_data_8bpp)} tiles using {len(colors)} colors.")
