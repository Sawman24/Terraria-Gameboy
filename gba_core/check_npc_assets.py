import os
from PIL import Image

assets = [
    r"c:\Users\sawye\Desktop\Terraria Beta\GBA_Assets\NPCs\NPC_1.png",
    r"c:\Users\sawye\Desktop\Terraria Beta\DumpedAssets\Items\Item_23.png"
]

for p in assets:
    if os.path.exists(p):
        img = Image.open(p)
        print(f"{os.path.basename(p)}: {img.size}")
    else:
        print(f"{os.path.basename(p)}: NOT FOUND at {p}")
