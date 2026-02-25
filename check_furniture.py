from PIL import Image
import os

def dump_sprite(path, name):
    if not os.path.exists(path):
        print(f"{name} NOT FOUND")
        return
    img = Image.open(path).convert("RGBA")
    print(f"--- {name} ({img.width}x{img.height}) ---")
    for y in range(min(img.height, 20)):
        row = ""
        for x in range(min(img.width, 30)):
            r,g,b,a = img.getpixel((x,y))
            if a < 10: row += "."
            elif r > 200 and g < 100: row += "R" # highlight Red-ish
            else: row += "X"
        print(f"{y:02}: {row}")

dump_sprite('GBA_Assets/Tiles/Tiles_18.png', 'Workbench')
dump_sprite('GBA_Assets/Tiles/Tiles_21.png', 'Chest')
dump_sprite('GBA_Assets/Tiles/Tiles_17.png', 'Furnace')
