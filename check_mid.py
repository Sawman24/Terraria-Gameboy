from PIL import Image
img = Image.open('GBA_Assets/Tiles/Tiles_18.png').convert("RGBA")
for x in range(7, 12):
    print(f"Col {x} alpha:", [img.getpixel((x, y))[3] for y in range(10)])
