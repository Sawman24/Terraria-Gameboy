import os
from PIL import Image

path = r"c:\Users\sawye\Desktop\Terraria Beta\GBA_Assets\Misc\Tree_Tops.png"
img = Image.open(path).convert("RGBA")
chars = ' .:-=+*#%@'
for y in range(img.height):
    row = ""
    for x in range(img.width):
        r, g, b, a = img.getpixel((x, y))
        if a < 128:
            row += "  "
        else:
            idx = int((r+g+b) / 3 / 256 * len(chars))
            row += chars[min(len(chars)-1, idx)] * 2
    print(row)
