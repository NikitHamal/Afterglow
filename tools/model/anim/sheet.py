# Contact sheet of clip previews: python3 sheet.py out.jpg [view,view] [clip,clip,...]
import sys, os
from PIL import Image, ImageDraw
P = "/root/agb/native/previews/"
out = sys.argv[1]
views = sys.argv[2].split(",") if len(sys.argv) > 2 else ["side", "front"]
clips = sys.argv[3].split(",") if len(sys.argv) > 3 else sorted({f.rsplit("_", 1)[0] for f in os.listdir(P) if f.endswith(".png")})
tiles = []
for c in clips:
    for v in views:
        p = P + f"{c}_{v}.png"
        if os.path.exists(p):
            im = Image.open(p).convert("RGB").resize((480, 270))
            ImageDraw.Draw(im).text((6, 6), f"{c} {v}", fill=(255, 40, 40)); tiles.append(im)
cols = 4; rows = max(1, (len(tiles) + cols - 1) // cols)
sheet = Image.new("RGB", (cols * 480, rows * 270), (40, 40, 40))
for i, im in enumerate(tiles): sheet.paste(im, ((i % cols) * 480, (i // cols) * 270))
sheet.save(out, quality=82); print(out, sheet.size)
