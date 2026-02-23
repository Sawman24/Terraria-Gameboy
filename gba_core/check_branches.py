import os
from PIL import Image

path = r"C:\Users\sawye\Desktop\Terraria Beta\GBA_Assets\Misc\Tree_Branches.png"
if os.path.exists(path):
    img = Image.open(path).convert("RGBA")
    print(f"Image size: {img.size}")
    # Let's see some basic info or a small crop
    # Based on the file name, it might be a sequence of branches.
    # I'll output a small grid of pixels to get an idea of the layout if small, 
    # or just trust the user if it's obvious.
    # Actually, I'll just print dimensions and maybe check for common 8x8 or 16x16 tiled patterns.
