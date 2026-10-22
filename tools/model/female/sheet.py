#!/usr/bin/env python3
# contact sheet: sheet.py <out.jpg> <height> <png>...
import sys, os
from PIL import Image
out, h = sys.argv[1], int(sys.argv[2])
ims = [Image.open(p).convert("RGB") for p in sys.argv[3:] if os.path.exists(p)]
ims = [i.resize((int(i.width * h / i.height), h), Image.LANCZOS) for i in ims]
c = Image.new("RGB", (sum(i.width for i in ims), h)); x = 0
for i in ims: c.paste(i, (x, 0)); x += i.width
c.save(out, quality=88); print(c.size)
