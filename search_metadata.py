def search_mip(filepath):
    with open(filepath, 'rb') as f:
        data = f.read()
        sig = b'\x01\x00\x00\x00'
        index = -1
        while True:
            index = data.find(sig, index + 1)
            if index == -1: break
            # Check if Format (4 bytes before index-8) is 0
            # Wait, Format is 4 bytes, Width 4, Height 4, MipCount 4
            # So Format is at index - 12
            if index >= 12:
                format_val = data[index-12:index-8]
                width = data[index-8:index-4]
                height = data[index-4:index]
                data_size = data[index+4:index+8]
                print(f"Index {index}: Format={format_val.hex()}, Width={width.hex()}, Height={height.hex()}, DataSize={data_size.hex()}")

if __name__ == '__main__':
    search_mip(r'c:\Users\sawye\Desktop\Terraria Beta\Terraria Beta\Content\Images\Item_0.xnb')
