from PIL import Image
img = Image.open('GBA_Assets/Tiles/Tiles_17.png').convert("RGBA")
for y in range(img.height):
    row = ""
    for x in range(img.width):
        a = img.getpixel((x, y))[3]
        row += "X" if a > 128 else "."
    print(f"{y:02}: {row}")
