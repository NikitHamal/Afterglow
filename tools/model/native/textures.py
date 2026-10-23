# Texture set for the native characters: 2K normal/ORM (glb swaps) + the three skin-tone albedos (4K JPEG).
#   python3 textures.py           -> /root/agb/native/tex/{vivian,marcus}/...
import os, json
from PIL import Image
OUT = "/root/agb/native/tex"
TONES = ("porcelain", "fair", "honey", "olive", "caramel", "bronze", "deep", "ebony")     # lightest -> darkest (missing ones are skipped)
SRC = {"vivian": "/root/agb/out/tex", "marcus": "/root/agb/male/tex"}
for who, src in SRC.items():
    d = f"{OUT}/{who}"; os.makedirs(d, exist_ok=True); swaps = {}
    for f in sorted(os.listdir(src)):
        p = f"{src}/{f}"; low = f.lower()
        if not low.endswith((".jpg", ".png")): continue
        if ("normal" in low or "orm" in low) and "2k" not in low:
            im = Image.open(p)
            if max(im.size) > 2048:
                o = f.rsplit(".", 1)[0] + "_2k.jpg"
                im.convert("RGB").resize((2048, 2048), Image.LANCZOS).save(f"{d}/{o}", quality=90)
                swaps[f.rsplit(".", 1)[0]] = o
        for tone in TONES:
            if f.startswith(f"{who}_skin_{tone}") and low.endswith(".jpg") and "game" not in low:
                Image.open(p).convert("RGB").save(f"{d}/{who}_skin_{tone}.jpg", quality=88, optimize=True, progressive=True)
            if f.startswith(f"{who}_genital_{tone}") and low.endswith(".jpg"):
                Image.open(p).convert("RGB").save(f"{d}/{who}_genital_{tone}.jpg", quality=88, optimize=True)
    import glob
    teeth = glob.glob("/root/agb/mh_assets/teeth/teeth_base/*.png")[0]
    Image.open(teeth).convert("RGB").resize((1024, 1024), Image.LANCZOS).save(f"{d}/teeth_1k.jpg", quality=88); swaps["teeth"] = "teeth_1k.jpg"
    tongue = glob.glob("/root/agb/mh_assets/tongue/tongue01/*.png")[0]
    Image.open(tongue).convert("RGB").resize((512, 512), Image.LANCZOS).save(f"{d}/tongue_512.jpg", quality=88); swaps["tongue01"] = "tongue_512.jpg"
    json.dump(swaps, open(f"{d}/swaps.json", "w"), indent=1)
    print(who, "swaps", swaps, "files", sorted(os.listdir(d)))
