import os
from PIL import Image

in_dir = "GBA_Assets/Player"
hud_path = "GBA_Assets/Misc/Heart.png"
out_h = "gba_core/sprite_gfx.h"

# Components that make up a player (bottom to top)
LAYERS_STATIC = [
    ("Player_Head.png", (255, 200, 150)),      # tinted skin
    ("Player_Eye_Whites.png", (255, 255, 255)),
    ("Player_Eyes.png", (0, 0, 0)),
    ("Player_Hair.png", (130, 70, 30)),        # brown hair
]
LAYERS_ANIM = [
    ("Player_Undershirt2.png", (255, 255, 255)), # Back sleeve
    ("Player_Hands2.png", (255, 200, 150)),      # Back hand
    ("Player_Undershirt.png", (255, 255, 255)),  # Front sleeve
    ("Player_Shirt.png", (200, 50, 50)),         # red shirt
    ("Player_Pants.png", (50, 50, 200)),         # blue pants
    ("Player_Shoes.png", (100, 100, 100)),
    ("Player_Hands.png", (255, 200, 150)),       # Front hand
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

# We'll just extract Frame 0 for the player to ensure perfection. 
# We can do bobbing animation with code!
all_tiles_8bpp = []

img_player = Image.new("RGBA", (32, 32), (0,0,0,0))
offset_x = 8
offset_y = 6
for name, tint in LAYERS_STATIC:
    path = os.path.join(in_dir, name)
    if os.path.exists(path):
        layer = Image.open(path).convert("RGBA")
        crop = apply_tint(layer.crop((0, 0, 17, 25)), tint)
        img_player.alpha_composite(crop, (offset_x, offset_y))

for name, tint in LAYERS_ANIM:
    path = os.path.join(in_dir, name)
    if os.path.exists(path):
        layer = Image.open(path).convert("RGBA")
        crop = apply_tint(layer.crop((0, 0, 17, 25)), tint)
        img_player.alpha_composite(crop, (offset_x, offset_y))

# Helper to slice an RGBA image into 8x8 blocks of 8bpp map indices
def slice_to_tiles(img, tw, th):
    for ty in range(th):
        for tx in range(tw):
            tile = []
            for y in range(8):
                for x in range(8):
                    px = tx*8 + x
                    py = ty*8 + y
                    if px < img.width and py < img.height:
                        r,g,b,a = img.getpixel((px, py))
                    else:
                        a = 0
                    if a < 10 or (r == 247 and b == 249):
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

# Add Player (4x4 = 16 tiles)
print(f"Player start: {len(all_tiles_8bpp)}")
slice_to_tiles(img_player, 4, 4)

# --- 2. Extract Heart ---
print(f"Heart start (Full): {len(all_tiles_8bpp)}")
img_heart_full = Image.new("RGBA", (16, 16), (0,0,0,0))
if os.path.exists(hud_path):
    heart = Image.open(hud_path).convert("RGBA")
    img_heart_full.alpha_composite(heart, (2, 2))
# Add Full Heart (2x2 = 4 tiles)
slice_to_tiles(img_heart_full, 2, 2)

print(f"Heart start (Half): {len(all_tiles_8bpp)}")
img_heart_half = Image.new("RGBA", (16, 16), (0,0,0,0))
if os.path.exists(hud_path):
    heart = Image.open(hud_path).convert("RGBA")
    # Half heart: mask right half
    half_heart = heart.copy()
    for y in range(half_heart.height):
        for x in range(half_heart.width // 2, half_heart.width):
            half_heart.putpixel((x, y), (0, 0, 0, 0))
    img_heart_half.alpha_composite(half_heart, (2, 2))
slice_to_tiles(img_heart_half, 2, 2)

print(f"Heart start (Empty): {len(all_tiles_8bpp)}")
img_heart_empty = Image.new("RGBA", (16, 16), (0,0,0,0))
if os.path.exists(hud_path):
    heart = Image.open(hud_path).convert("RGBA")
    # Empty heart: just a dark outline (dimmed)
    empty_heart = Image.new("RGBA", heart.size, (0, 0, 0, 0))
    for y in range(heart.height):
        for x in range(heart.width):
            r, g, b, a = heart.getpixel((x, y))
            if a > 0:
                empty_heart.putpixel((x, y), (40, 20, 20, 200)) # Dark red dim
    img_heart_empty.alpha_composite(empty_heart, (2, 2))
slice_to_tiles(img_heart_empty, 2, 2)

# --- 3. Extract Cursor ---
print(f"Cursor (at {len(all_tiles_8bpp)}): {len(all_tiles_8bpp)}")
cursor_path = "GBA_Assets/Misc/Cursor.png"
img_cursor = Image.new("RGBA", (8, 8), (0,0,0,0))
if os.path.exists(cursor_path):
    cursor = Image.open(cursor_path).convert("RGBA")
    img_cursor.alpha_composite(cursor, (0, 0))
# Add Cursor (1x1 = 1 tile)
slice_to_tiles(img_cursor, 1, 1)

# --- 4. Extract Inventory Box & Select ---
inv_path = "GBA_Assets/Misc/Inventory_Back.png"
img_inv = Image.new("RGBA", (32, 32), (0,0,0,0))
if os.path.exists(inv_path):
    inv = Image.open(inv_path).convert("RGBA")
    img_inv.alpha_composite(inv, (3, 3)) # 26x26, so offset by 3 to center
# Add Inventory Back (4x4 = 16 tiles)
slice_to_tiles(img_inv, 4, 4)

sel_path = "GBA_Assets/Misc/Inventory_Select.png"
img_sel = Image.new("RGBA", (32, 32), (0,0,0,0))
if os.path.exists(sel_path):
    sel = Image.open(sel_path).convert("RGBA")
    img_sel.alpha_composite(sel, (3, 3)) # 26x26, offset by 3 to center
# Add Inventory Select (4x4 = 16 tiles)
slice_to_tiles(img_sel, 4, 4)

# --- 5. Extract Items (Tools & Blocks) ---
# Each tool gets a Normal and a Flipped (Facing Left) frame.
tools_to_flip = [
     "GBA_Assets/Items/Item_4.png", # Sword
     "GBA_Assets/Items/Item_1.png", # Pickaxe
     "GBA_Assets/Items/Item_10.png", # Axe
     "GBA_Assets/Items/Item_8.png", # Torch
]

for p in tools_to_flip:
    if os.path.exists(p):
        img = Image.open(p).convert("RGBA")
        if img.width > 16 or img.height > 16:
            img = img.resize((16, 16), resample=Image.NEAREST)
        
        # Normal
        padded = Image.new("RGBA", (16, 16), (0,0,0,0))
        padded.alpha_composite(img, ((16-img.width)//2, (16-img.height)//2))
        slice_to_tiles(padded, 2, 2)
        
        # Flipped
        flipped = padded.transpose(Image.FLIP_LEFT_RIGHT)
        slice_to_tiles(flipped, 2, 2)

# Add remaining blocks and items (16x16 each)
misc_items = [
     "GBA_Assets/Items/Item_2.png", # Dirt
     "GBA_Assets/Items/Item_3.png", # Stone
     ("GBA_Assets/Tiles/Tiles_5.png", (1, 0, 9, 8)),   # Wood
     "GBA_Assets/Items/Item_9.png", # Planks
     ("GBA_Assets/Tiles/Tiles_59.png", (9, 9, 17, 17)), # Mud
     ("GBA_Assets/Tiles/Tiles_60.png", (9, 0, 17, 8)), # Jungle Grass
     "GBA_Assets/Items/Item_27.png", # Acorn
     "GBA_Assets/Items/Item_172.png", # Ash
]

for entry in misc_items:
    p = entry[0] if isinstance(entry, tuple) else entry
    crop_box = entry[1] if isinstance(entry, tuple) else None
    
    crop = None
    if os.path.exists(p):
        img = Image.open(p).convert("RGBA")
        if crop_box:
            crop = img.crop(crop_box)
            crop = crop.resize((16, 16), resample=Image.NEAREST)
        else:
            crop = img 
    
    padded = Image.new("RGBA", (16, 16), (0,0,0,0))
    if crop:
        if crop.width > 16 or crop.height > 16:
            crop = crop.resize((16, 16), resample=Image.NEAREST)
        # Filter alpha 
        filtered = Image.new("RGBA", (crop.width, crop.height))
        for y in range(crop.height):
            for x in range(crop.width):
                r,g,b,a = crop.getpixel((x,y))
                if a < 10 or (r == 247 and b == 249) or (r==255 and g==255 and b==0):
                    filtered.putpixel((x,y), (0,0,0,0))
                else:
                    filtered.putpixel((x,y), (r,g,b,255))
        padded.alpha_composite(filtered, ((16-filtered.width)//2, (16-filtered.height)//2))
    
    slice_to_tiles(padded, 2, 2)
            
# --- 6. Extract Slime NPC ---
npc_path = "GBA_Assets/NPCs/NPC_1.png"
if os.path.exists(npc_path):
    slime_raw = Image.open(npc_path).convert("RGBA")
    slime = apply_tint(slime_raw, (100, 255, 100))
    # 16x26 total, 2 frames of 16x13
    for f in range(2):
        frame = slime.crop((0, f*13, 16, (f+1)*13))
        # Center in 16x16
        padded = Image.new("RGBA", (16, 16), (0,0,0,0))
        padded.alpha_composite(frame, (0, 3)) # 16-13=3 to align bottom
        slice_to_tiles(padded, 2, 2)

# --- 7b. Extract Cave Slime NPC (NPC_16) ---
npc16_path = "GBA_Assets/NPCs/NPC_16.png"
if os.path.exists(npc16_path):
    slime_raw = Image.open(npc16_path).convert("RGBA")
    # Tint it RED as requested
    slime = apply_tint(slime_raw, (255, 100, 100))
    # NPC_16 is 22x33. Crop to a clean frame
    for f in range(2):
        frame = slime.crop((0, f*16, 22, (f+1)*16))
        # Center 22x16 in 32x32? No, let's stick to 16x16 for slime for now if possible,
        # or resize it. Let's resize it to 16x16 to keep it simple.
        frame_small = frame.resize((16, 16), resample=Image.NEAREST)
        slice_to_tiles(frame_small, 2, 2)

# --- 8. Extract Gel Item ---
gel_path = "GBA_Assets/Items/Item_23.png"
if os.path.exists(gel_path):
    gel = Image.open(gel_path).convert("RGBA")
    gel_tinted = apply_tint(gel, (100, 255, 100))
    padded = Image.new("RGBA", (16, 16), (0,0,0,0))
    # 8x7 centered in 16x16
    padded.alpha_composite(gel_tinted, (4, 4))
    slice_to_tiles(padded, 2, 2)

# --- 8b. Extract Ores, Bars, Furniture ---
print(f"Extra items start (Copper Ore): {len(all_tiles_8bpp)}")
# Ores (2), Bars (2), Furniture (3) = 7 items
extra_items = [
     "GBA_Assets/Items/Item_12.png", # Copper Ore
     "GBA_Assets/Items/Item_11.png", # Iron Ore
     "GBA_Assets/Items/Item_20.png", # Copper Bar
     "GBA_Assets/Items/Item_22.png", # Iron Bar
     "GBA_Assets/Items/Item_36.png", # Workbench
     "GBA_Assets/Items/Item_33.png", # Furnace
     "GBA_Assets/Items/Item_48.png", # Chest
     "GBA_Assets/Items/Item_49.png", # Band of Regeneration
     "GBA_Assets/Items/Item_50.png", # Magic Mirror
     "GBA_Assets/Items/Item_53.png", # Cloud in a Bottle
     "GBA_Assets/Items/Item_18.png", # Depth Meter
     "GBA_Assets/Items/Item_128.png", # Rocket Boots
]

for p in extra_items:
    padded = Image.new("RGBA", (16, 16), (0,0,0,0))
    if os.path.exists(p):
        img = Image.open(p).convert("RGBA")
        if img.width > 16 or img.height > 16:
            img = img.resize((16, 16), resample=Image.NEAREST)
        
        # Filter alpha 
        filtered = Image.new("RGBA", (img.width, img.height))
        for y in range(img.height):
            for x in range(img.width):
                r,g,b,a = img.getpixel((x,y))
                if a < 10 or (r == 247 and b == 249) or (r==255 and g==255 and b==0):
                    filtered.putpixel((x,y), (0,0,0,0))
                else:
                    filtered.putpixel((x,y), (r,g,b,255))
        padded.alpha_composite(filtered, ((16-filtered.width)//2, (16-filtered.height)//2))
    slice_to_tiles(padded, 2, 2)

# --- 9. Extract Number Font ---
# 0-9 in a 3x5 format
font_data = [
    "111101101101111", # 0
    "010110010010111", # 1
    "111001111100111", # 2
    "111001011001111", # 3
    "101101111001001", # 4
    "111100111001111", # 5
    "111100111101111", # 6
    "111001010010010", # 7
    "111101111101111", # 8
    "111101111001111", # 9
]
for digit in font_data:
    img_digit = Image.new("RGBA", (8, 8), (0,0,0,0))
    for i, p in enumerate(digit):
        if p == '1':
            dx = i % 3
            dy = i // 3
            # drop shadow (black) drawn under white
            img_digit.putpixel((dx+2, dy+2), (0, 0, 0, 255))
            img_digit.putpixel((dx+1, dy+1), (255, 255, 255, 255))
    slice_to_tiles(img_digit, 1, 1)

# --- 10. Extract Sun (32x32 = 4x4 tiles, resized to 75%) ---
sun_sprite_p = "GBA_Assets/Misc/Sun.png"
if os.path.exists(sun_sprite_p):
    sun_raw = Image.open(sun_sprite_p).convert("RGBA")
    # Scale to 75% of 32x32 = 24x24
    sun_small = sun_raw.resize((24, 24), resample=Image.NEAREST)
    sun = Image.new("RGBA", (32, 32), (0,0,0,0))
    # Center 24x24 in 32x32 (offset 4,4)
    sun.alpha_composite(sun_small, (4, 4))
    slice_to_tiles(sun, 4, 4)
else:
    # 16 blank tiles
    for _ in range(16): 
        all_tiles_8bpp.append([0]*64)

# --- 11. Extract Smart Cursor ---
smart_cursor_path = "GBA_Assets/Misc/Smart_Cursor.png"
img_smart_cursor = Image.new("RGBA", (8, 8), (0,0,0,0))
if os.path.exists(smart_cursor_path):
    smart_cursor = Image.open(smart_cursor_path).convert("RGBA")
    img_smart_cursor.alpha_composite(smart_cursor, (0, 0))
slice_to_tiles(img_smart_cursor, 1, 1)

print("Generating sprite_gfx.h")
with open(out_h, "w") as f:
    f.write("#ifndef SPRITE_GFX_H\n#define SPRITE_GFX_H\n\n")
    
    pal_data = []
    for r, g, b in colors:
        rgb555 = ((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3)
        pal_data.append(rgb555)

    while len(pal_data) < 256:
        pal_data.append(0)
        
    f.write(f"const unsigned short sprite_palette[256] = {{\n")
    for i in range(0, 256, 16):
        chunk = pal_data[i:i+16]
        f.write("    " + ", ".join(f"0x{c:04X}" for c in chunk) + ",\n")
    f.write("};\n\n")
    
    f.write(f"const unsigned char sprite_graphics[{len(all_tiles_8bpp)*64}] __attribute__((aligned(4))) = {{\n")
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
