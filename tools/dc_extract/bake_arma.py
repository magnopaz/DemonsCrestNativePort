import json
from pathlib import Path

from PIL import Image

ROOT = Path(r"M:\Opencode\DemonsCrestNativePort")
SRC = ROOT / "docs/sprites/SNES - Demon's Crest - Enemies & Bosses - Arma.gif"
GEN = ROOT / "assets/generated"

BANDS = {
    "idle": (22, 66, 55, 108),
    "breath": (0, 148, 248, 312),
    "dive": (158, 252, 248, 312),
    "hurt": (283, 402, 248, 312),
}

im = Image.open(SRC).convert("RGB")
from collections import Counter
small = im.resize((im.width // 4, im.height // 4))
bg = Counter(small.getdata()).most_common(1)[0][0]
print("background", bg)

frames = {}
for clip, (x0, x1, y0, y1) in BANDS.items():
    crop = im.crop((x0, y0, x1, y1)).convert("RGBA")
    px = crop.load()
    for y in range(crop.height):
        for x in range(crop.width):
            r, g, b, a = px[x, y]
            if abs(r - bg[0]) + abs(g - bg[1]) + abs(b - bg[2]) <= 30:
                px[x, y] = (0, 0, 0, 0)
    bbox = crop.getbbox()
    crop = crop.crop(bbox)
    frames[clip] = crop
    print(clip, crop.size)

# Species 0xA5 (Arma): idle, breath, dive, hurt.
anim_path = GEN / "species_anim.json"
anim = json.loads(anim_path.read_text())
entries = []
for i, clip in enumerate(("idle", "breath", "dive", "hurt")):
    tex = f"enemy_a5_{i}.png"
    frames[clip].save(GEN / tex)
    entries.append({
        "texture": tex,
        "anchor_x": frames[clip].width // 2,
        "anchor_y": frames[clip].height,
        "duration": 8,
    })
anim["species"]["a5"] = {
    "species": 0xA5,
    "frames": entries,
    "source": "user sprite sheet (Spriters Resource, ripped by Lord Zymeth)",
}
anim_path.write_text(json.dumps(anim, indent=1))
print("species a5 frames:", [e["texture"] for e in entries])
