# Build a hair style on a character's game rig and write it as characters/hair/<name>.glb, or splice it into the
# character's own GLB (replacing the embedded hair).
#   blender -b <rig>.blend --python tools/model/hair/build_hair.py -- <style> <name> <out_dir> [--splice <src.glb> <dst.glb>]
#                                                                        [--role vivian|marcus] [--seed N]
import sys, os, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import hairkit, hairio, styles

argv = sys.argv[sys.argv.index("--") + 1:]
style, name, out = argv[0], argv[1], argv[2]
role = argv[argv.index("--role") + 1] if "--role" in argv else "vivian"
seed = int(argv[argv.index("--seed") + 1]) if "--seed" in argv else 11
t0 = time.time(); rng = np.random.default_rng(seed)
names = {"vivian": ("vivian_body", "vivian_eyes", "VivianRig"), "marcus": ("marcus_body", "marcus_eyes", "MarcusRig")}[role]
h = hairkit.Head(*names, rng)
b = styles.STYLES[style](h, rng)
prims = b.arrays()
morph_names = ("weight_up", "weight_dn")
for p in prims:
    if p is not None: p["morphs"] = h.morph_deltas(p["pos"], morph_names)
atlas = hairio.png_bytes(hairio.strand_atlas(np.random.default_rng(seed + 1)))
cap = hairio.png_bytes(hairio.cap_texture(np.random.default_rng(seed + 2), coily=(style == "puff")))
os.makedirs(out, exist_ok=True)
nv = sum(len(p["pos"]) for p in prims if p is not None); nt = sum(len(p["tris"]) for p in prims if p is not None)
if "--splice" in argv:
    i = argv.index("--splice"); src, dst = argv[i + 1], argv[i + 2]
    hairio.splice_hair(src, dst, f"{role}_hair", prims, morph_names, atlas, cap)
    print(f"[build_hair] spliced {style} into {dst}: {nv} verts, {nt} tris, {os.path.getsize(dst)/1e6:.1f} MB ({time.time()-t0:.0f}s)", flush=True)
else:
    joints = sorted({j for p in prims if p is not None for wd in p["w"] for j in wd})
    joints = [hairkit.HEAD] + [j for j in joints if j != hairkit.HEAD]
    path = os.path.join(out, f"{name}.glb")
    hairio.write_style_glb(path, name, prims, morph_names, atlas, cap, joints)
    print(f"[build_hair] {path}: {nv} verts, {nt} tris, {os.path.getsize(path)/1e6:.1f} MB, joints {joints} ({time.time()-t0:.0f}s)", flush=True)
