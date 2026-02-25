from PIL import Image
img = Image.open('GBA_Assets/Tiles/Tiles_18.png').convert("RGBA")
for y in range(img.height):
    row = "".join(["X" if img.getpixel((x,y))[3]>128 else "." for x in range(img.width)])
    print(f"{y:02}: {row}")
