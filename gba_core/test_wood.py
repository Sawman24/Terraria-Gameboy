import os
from PIL import Image

blocks = [
     r"c:\Users\sawye\Desktop\Terraria Beta\GBA_Assets\Tiles\Tiles_5.png", # Wood
]
for p in blocks:
    if os.path.exists(p):
        img = Image.open(p).convert("RGBA")
        crop = img.crop((1, 0, 9, 8))
        crop = crop.resize((16, 16), resample=Image.NEAREST)
        
        filtered = Image.new("RGBA", (16, 16))
        for y in range(16):
            for x in range(16):
                r,g,b,a = crop.getpixel((x,y))
                if a < 128 or (r == 247 and b == 249) or (r==255 and g==255 and b==0):
                    filtered.putpixel((x,y), (0,0,0,0))
                else:
                    filtered.putpixel((x,y), (r,g,b,255))
        
        chars = ' .:-=+*#%@'
        print(f"--- Block {os.path.basename(p)} ---")
        for y in range(16):
            row = ""
            for x in range(16):
                r,g,b,a = filtered.getpixel((x,y))
                if a < 128:
                    row += "  "
                else:
                    idx = int((r+g+b) / 3 / 256 * len(chars))
                    row += chars[min(len(chars)-1, idx)] * 2
            print(row)
