import os
from PIL import Image

path = r"C:\Users\sawye\Desktop\Terraria Beta\GBA_Assets\Misc\Tree_Branches.png"
if os.path.exists(path):
    img = Image.open(path).convert("RGBA")
    print(f"Image size: {img.size}")
    
    # Simple ASCII art to see where the branches are
    # Scale down or just print a grid
    for y in range(0, img.height, 2):
        line = ""
        for x in range(0, img.width, 2):
            r,g,b,a = img.getpixel((x, y))
            if a > 0:
                line += "#"
            else:
                line += " "
        print(line)
