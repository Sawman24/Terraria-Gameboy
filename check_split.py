from PIL import Image
img = Image.open('GBA_Assets/Tiles/Tiles_18.png').convert("RGBA")
print("Col 8 alpha:", [img.getpixel((8, y))[3] for y in range(10)])
print("Col 9 alpha:", [img.getpixel((9, y))[3] for y in range(10)])
