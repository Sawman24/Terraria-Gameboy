import lzma
import os

def test_lzma(filepath, offset):
    with open(filepath, 'rb') as f:
        f.seek(offset)
        data = f.read()
        try:
            # LZMA in XNB (MonoGame) usually has a slightly different header
            # But let's try raw
            decompressed = lzma.decompress(data)
            print(f"Success at offset {offset}: {len(decompressed)} bytes")
            return decompressed
        except:
            pass
    return None

if __name__ == '__main__':
    path = r'c:\Users\sawye\Desktop\Terraria Beta\Terraria Beta\Content\Images\Item_0.xnb'
    for i in range(10, 20):
        test_lzma(path, i)
