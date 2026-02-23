import os

def check_all_headers(directory):
    headers = {
        'PNG': b'\x89PNG',
        'JPEG': b'\xff\xd8\xff',
        'BMP': b'BM',
        'GIF': b'GIF8',
    }
    for filename in os.listdir(directory):
        if filename.endswith('.xnb'):
            filepath = os.path.join(directory, filename)
            with open(filepath, 'rb') as f:
                content = f.read()
                for name, sig in headers.items():
                    index = content.find(sig)
                    if index != -1:
                        print(f"{filename}: Found {name} at {index}")

if __name__ == '__main__':
    check_all_headers(r'c:\Users\sawye\Desktop\Terraria Beta\Terraria Beta\Content\Images')
