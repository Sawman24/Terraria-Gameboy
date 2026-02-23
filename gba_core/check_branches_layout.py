import os
from PIL import Image

path = r"C:\Users\sawye\Desktop\Terraria Beta\GBA_Assets\Misc\Tree_Branches.png"
if os.path.exists(path):
    img = Image.open(path).convert("RGBA")
    # Save a few crops to see what they look like? 
    # No, I can't see them. I'll just analyze alpha.
    
    # Check left half (0-20) and right half (21-41) for each 21px high row
    for row in range(3):
        y0 = row * 21
        y1 = y0 + 21
        # Left branch (roughly 0-20)
        left_has_alpha = False
        for y in range(y0, y1):
            for x in range(0, 20):
                if img.getpixel((x, y))[3] > 0:
                    left_has_alpha = True
                    break
        # Right branch (roughly 22-41)
        right_has_alpha = False
        for y in range(y0, y1):
            for x in range(22, 42):
                if img.getpixel((x, y))[3] > 0:
                    right_has_alpha = True
                    break
        print(f"Row {row}: Left Branch={left_has_alpha}, Right Branch={right_has_alpha}")
