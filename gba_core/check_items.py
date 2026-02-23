import os
from PIL import Image

in_dir = r"c:\Users\sawye\Desktop\Terraria Beta\GBA_Assets\Items"

chars = ' .:-=+*#%@'
for i in range(20):
    path = os.path.join(in_dir, f"Item_{i}.png")
    if os.path.exists(path):
        img = Image.open(path).convert("RGBA")
        print(f"--- Item_{i}.png ({img.width}x{img.height}) ---")
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
