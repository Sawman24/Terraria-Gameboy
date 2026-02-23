import zlib
import os

def test_zlib(filepath, offset):
    with open(filepath, 'rb') as f:
        f.seek(offset)
        data = f.read()
        try:
            decompressed = zlib.decompress(data)
            print(f"Success at offset {offset}: {len(decompressed)} bytes")
            return decompressed
        except Exception as e:
            # print(f"Failed at offset {offset}: {e}")
            pass
    return None

if __name__ == '__main__':
    path = r'c:\Users\sawye\Desktop\Terraria Beta\Terraria Beta\Content\Images\Cursor.xnb'
    for i in range(10, 20):
        test_zlib(path, i)
