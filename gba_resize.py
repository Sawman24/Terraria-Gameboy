"""
Scale all extracted Terraria sprites to 50% for GBA use.
Uses nearest-neighbor to preserve pixel art crispness.
"""

from PIL import Image
import os

INPUT_DIR = r"c:\Users\sawye\Desktop\Terraria Beta\DumpedAssets"
OUTPUT_DIR = r"c:\Users\sawye\Desktop\Terraria Beta\GBA_Assets"

SCALE = 0.5

CATEGORIES = ["Items", "NPCs", "Tiles", "Walls", "Gore", "Projectiles",
              "Player", "Backgrounds", "Misc"]

if __name__ == "__main__":
    print(f"=== Scaling all assets to {int(SCALE*100)}% ===\n")
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    total = 0

    for cat in CATEGORIES:
        in_dir = os.path.join(INPUT_DIR, cat)
        out_dir = os.path.join(OUTPUT_DIR, cat)
        if not os.path.exists(in_dir):
            continue
        os.makedirs(out_dir, exist_ok=True)

        files = [f for f in os.listdir(in_dir) if f.endswith(".png")]
        for f in sorted(files):
            img = Image.open(os.path.join(in_dir, f)).convert("RGBA")
            w, h = img.size
            nw = max(1, int(w * SCALE))
            nh = max(1, int(h * SCALE))
            resized = img.resize((nw, nh), Image.NEAREST)
            resized.save(os.path.join(out_dir, f))
            total += 1

        print(f"  {cat}: {len(files)} sprites  ({w}x{h} -> {nw}x{nh} example)")

    print(f"\nDone! {total} sprites saved to {OUTPUT_DIR}")
