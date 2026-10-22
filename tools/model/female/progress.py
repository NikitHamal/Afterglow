#!/usr/bin/env python3
"""Progress feed for the molab dashboard (Afterglow · Vivian model build).
  progress.py set <stage> <todo|active|done|fail> [note]
  progress.py frames <png>...        -> adds JPEG thumbnails to out/frames + rebuilds the latest contact sheet
  progress.py log <text>
"""
import json, os, sys, time
ROOT = "/root/agb/out"; P = f"{ROOT}/progress.json"; FR = f"{ROOT}/frames"
STAGES = [
    ("tools", "Toolchain", "Blender 4.2 LTS + MPFB 2 + MakeHuman CC0 assets"),
    ("body", "Adult body", "Woman ~40, curvy mature figure (MakeHuman age 0.62)"),
    ("look", "Skin · hair · eyes", "Middle-aged skin, long hair, brows, lashes, eyes"),
    ("rig", "Game skeleton", "MMD bones matching Afterglow's driver + weights"),
    ("posetest", "Deformation test", "Bend arms, legs, spine: check skin weights"),
    ("export", "Web GLB", "Optimised GLB with compressed textures"),
    ("game", "In-game", "New preset wired into Afterglow 3D, in-game shots"),
    ("push", "Shipped", "Pushed to GitHub main"),
]
def load():
    try: return json.load(open(P))
    except Exception:
        return {"title": "Afterglow · Vivian (adult 3D model)", "stages": [dict(id=i, name=n, desc=d, status="todo", note="") for i, n, d in STAGES], "log": [], "frames": [], "updated": 0}
def save(s):
    s["updated"] = time.time(); tmp = P + ".tmp"; json.dump(s, open(tmp, "w")); os.replace(tmp, P)
def main():
    os.makedirs(FR, exist_ok=True); s = load(); cmd = sys.argv[1]
    if cmd == "set":
        for st in s["stages"]:
            if st["id"] == sys.argv[2]:
                st["status"] = sys.argv[3]; st["note"] = " ".join(sys.argv[4:]) or st.get("note", "")
    elif cmd == "log":
        s["log"] = (s["log"] + [time.strftime("%H:%M:%S ") + " ".join(sys.argv[2:])])[-40:]
    elif cmd == "frames":
        from PIL import Image
        thumbs = []
        for src in sys.argv[2:]:
            if not os.path.exists(src): continue
            im = Image.open(src).convert("RGB"); im.thumbnail((520, 520))
            name = f"{int(time.time()*1000)}_{os.path.splitext(os.path.basename(src))[0]}.jpg"
            im.save(f"{FR}/{name}", quality=86); thumbs.append(name)
        s["frames"] = (s["frames"] + thumbs)[-48:]
    save(s)
if __name__ == "__main__": main()
