from PIL import Image

def draw_sheet(path, rows=4, cols=8):
    img = Image.open(path).convert('RGBA')
    print('Width:', img.width, 'Height:', img.height)
    chars = ' .:-=+*#%@'
    out = ""
    for sy in range(rows):
        for y in range(8):
            row_str = ''
            for sx in range(cols):
                for x in range(8):
                    px = sx*9 + x
                    py = sy*9 + y
                    if px >= img.width or py >= img.height:
                        row_str += ' '
                        continue
                    r, g, b, a = img.getpixel((px, py))
                    if a < 128 or (r == 247 and b == 249) or (r == 255 and g == 255 and b == 0):
                        row_str += ' '
                    else:
                        idx = int((r+g+b) / 3 / 256 * len(chars))
                        row_str += chars[min(len(chars)-1, idx)]
                row_str += '|'
            out += row_str + "\n"
        out += '-' * (cols * 9) + "\n"
    print(out)

print("Tree_Tops:")
draw_sheet(r'c:\Users\sawye\Desktop\Terraria Beta\GBA_Assets\Misc\Tree_Tops.png', 4, 8)
