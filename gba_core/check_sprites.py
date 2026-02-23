import os
from PIL import Image

in_dir = r"c:\Users\sawye\Desktop\Terraria Beta\GBA_Assets\Player"
files = sorted(os.listdir(in_dir))
for f in files:
    if f.endswith(".png"):
        img = Image.open(os.path.join(in_dir, f))
        print(f"{f}: {img.width}x{img.height}")

