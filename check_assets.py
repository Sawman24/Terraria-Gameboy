from PIL import Image
import os

def check(name, path):
    if not os.path.exists(path):
        print(f"{name}: {path} not found")
        return
    img = Image.open(path).convert("RGBA")
    print(f"{name} ({path}): {img.size}")
    # Find bounding box of non-transparent pixels
    bbox = img.getbbox()
    print(f"  bbox: {bbox}")

check("Furnace", "GBA_Assets/Tiles/Tiles_17.png")
check("Workbench", "GBA_Assets/Tiles/Tiles_18.png")
check("Chest", "GBA_Assets/Tiles/Tiles_21.png")
