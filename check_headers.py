import os

def check_headers(filepath):
    with open(filepath, 'rb') as f:
        content = f.read()
        headers = {
            'PNG': b'\x89PNG\r\n\x1a\n',
            'JPEG': b'\xff\xd8\xff',
            'GIF': b'GIF8',
            'BMP': b'BM',
            'DDS': b'DDS ',
            'ZLIB': b'\x78\x9c', # Low compression
            'ZLIB_ALT': b'\x78\x5e',
            'ZLIB_BEST': b'\x78\x01',
        }
        for name, sig in headers.items():
            index = content.find(sig)
            if index != -1:
                print(f"Found {name} header at index {index}")

if __name__ == '__main__':
    check_headers(r'c:\Users\sawye\Desktop\Terraria Beta\Terraria Beta\Content\Images\Item_0.xnb')
    check_headers(r'c:\Users\sawye\Desktop\Terraria Beta\Terraria Beta\Content\Images\Logo.xnb')
