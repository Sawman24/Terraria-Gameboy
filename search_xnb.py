def find_string(filepath, search_str):
    with open(filepath, 'rb') as f:
        content = f.read()
        index = content.find(search_str.encode('ascii'))
        if index != -1:
            print(f"Found '{search_str}' at index {index}")
        else:
            print(f"'{search_str}' not found")

if __name__ == '__main__':
    find_string(r'c:\Users\sawye\Desktop\Terraria Beta\Terraria Beta\Content\Images\Item_0.xnb', 'Reader')
    find_string(r'c:\Users\sawye\Desktop\Terraria Beta\Terraria Beta\Content\Images\Item_0.xnb', 'Texture')
    find_string(r'c:\Users\sawye\Desktop\Terraria Beta\Terraria Beta\Content\Images\Item_0.xnb', 'Xna')
