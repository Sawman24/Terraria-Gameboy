import os
from PIL import Image

in_dir = r"c:\Users\sawye\Desktop\Terraria Beta\GBA_Assets\Player"

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

# Print ASCII
chars = ' .:-=+*#%@'
for y in range(32):
    row = ""
    for x in range(32):
        r, g, b, a = img_player.getpixel((x, y))
        if a < 128:
            row += "  "
        else:
            idx = int((r+g+b) / 3 / 256 * len(chars))
            row += chars[min(len(chars)-1, idx)] * 2
    print(f"{y:02d}: {row}")

# Find bounding box
min_x = 32
max_x = 0
min_y = 32
max_y = 0
for y in range(32):
    for x in range(32):
        r, g, b, a = img_player.getpixel((x, y))
        if a >= 128:
            if x < min_x: min_x = x
            if x > max_x: max_x = x
            if y < min_y: min_y = y
            if y > max_y: max_y = y

print(f"Bounding Box: X: {min_x}-{max_x} (Width: {max_x - min_x + 1}), Y: {min_y}-{max_y} (Height: {max_y - min_y + 1})")
