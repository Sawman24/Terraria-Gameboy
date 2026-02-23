from PIL import Image
import math

base_img = Image.open(r"c:\Users\sawye\Desktop\Terraria Beta\GBA_Assets\Items\Item_4.png").convert("RGBA")
padded = Image.new("RGBA", (32, 32), (0,0,0,0))
padded.alpha_composite(base_img, (16, 0)) # bottom-left of 16x16 is at (16,16)

chars = ' .:-=+*#%@'
for angle in [45, 0, -45]:
    rot = padded.rotate(angle, resample=Image.NEAREST)
    print(f"--- Angle {angle} ---")
    for y in range(32):
        row = ""
        for x in range(32):
            r, g, b, a = rot.getpixel((x, y))
            if a < 128:
                row += "  "
            else:
                idx = int((r+g+b) / 3 / 256 * len(chars))
                row += chars[min(len(chars)-1, idx)] * 2
        print(row)
