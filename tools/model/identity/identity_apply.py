# A new face on a proven rig, step 2 of 2: the MPFB identity delta (identity.py) onto the base's game rig and onto the
# shipped GLB.  blender -b /root/agb/models/<base>_game.blend --python identity_apply.py -- <id> <id.npz> <base.glb> <out_dir>
# Writes <out_dir>/<id>_game.blend (the base rig with the new face: hair styles are grown on it) and
# <out_dir>/<id>_identity.bin, the per-vertex position + normal deltas of the GLB's own meshes, which the engine adds to
# the base GLB at load (scene/character.cpp): same skeleton, clips, UVs, morphs and outfits — only the face differs.
# Every vertex of the base keeps its index below 13380 (MakeHuman body vertices; the anatomy subdivision appends), which
# is checked here (similarity fit residual) before anything is written.
import bpy, sys, os, json, struct
import numpy as np
from mathutils.kdtree import KDTree

argv = sys.argv[sys.argv.index("--") + 1:]
MID, NPZ, GLB, OUT = argv[:4]
HERE = os.path.dirname(os.path.abspath(__file__))
R = json.load(open(os.path.join(HERE, "recipes.json")))
M = R["models"][MID]; BASE = R["bases"][M["base"]]; MESH = BASE["meshes"]
Z = np.load(NPZ)
def log(*a): print("[identity_apply]", *a, flush=True)
NBODY = 13380

def keys_co(o):
    me = o.data
    if not me.shape_keys:
        a = np.empty(len(me.vertices) * 3, np.float32); me.vertices.foreach_get("co", a); return {"__base__": a.reshape(-1, 3)}
    out = {}
    for kb in me.shape_keys.key_blocks:
        a = np.empty(len(me.vertices) * 3, np.float32); kb.data.foreach_get("co", a); out[kb.name] = a.reshape(-1, 3)
    return out
def basis(o):
    k = keys_co(o); return k.get("Basis", k.get("__base__", next(iter(k.values())))).astype(np.float64)
def add_all(o, d):
    me = o.data
    if not me.shape_keys:
        a = np.empty(len(me.vertices) * 3, np.float32); me.vertices.foreach_get("co", a)
        me.vertices.foreach_set("co", (a.reshape(-1, 3) + d).astype(np.float32).ravel()); me.update(); return
    for kb in me.shape_keys.key_blocks:
        a = np.empty(len(me.vertices) * 3, np.float32); kb.data.foreach_get("co", a)
        kb.data.foreach_set("co", (a.reshape(-1, 3) + d).astype(np.float32).ravel())
    me.vertices.foreach_set("co", (basis(o) + 0).astype(np.float32).ravel())
    me.update()
def normals(o):
    me = o.data; me.update()
    a = np.empty(len(me.vertices) * 3, np.float32); me.vertices.foreach_get("normal", a); return a.reshape(-1, 3).astype(np.float64)

def umeyama(src, dst):
    ms, md = src.mean(0), dst.mean(0); S, Dd = src - ms, dst - md
    U, sig, Vt = np.linalg.svd(Dd.T @ S / len(src))
    d = np.sign(np.linalg.det(U @ Vt)); E = np.diag([1, 1, d])
    Rm = U @ E @ Vt; s = (sig * np.diag(E)).sum() / (S ** 2).sum() * len(src)
    return s, Rm, md - s * Rm @ ms

body = bpy.data.objects[MESH["body"]]
G = basis(body)
B0 = Z["B0"].astype(np.float64)
n = min(NBODY, len(G))
s, Rm, t = umeyama(B0[:n], G[:n])
res = np.linalg.norm((s * (Rm @ B0[:n].T)).T + t - G[:n], axis=1)
log(f"MakeHuman -> game: scale {s:.4f}, residual median {np.median(res)*1000:.3f} mm, p99 {np.percentile(res, 99)*1000:.3f} mm")
head_idx = np.where(Z["mask"][:n] > 0.5)[0]
log(f"  head residual max {res[head_idx].max()*1000:.3f} mm")
if np.percentile(res[head_idx], 99) > 0.5e-3:
    raise SystemExit("vertex correspondence broken: refusing to write")
def to_game(d): return (s * (Rm @ d.T)).T
Dg = np.zeros_like(G); Dg[:n] = to_game(Z["D"][:n].astype(np.float64))

objs = {"body": body}
for sub in ("eyes", "eyebrows", "eyelashes", "teeth", "tongue"):
    objs[sub] = bpy.data.objects[MESH[sub]]
before = {k: (basis(o), normals(o)) for k, o in objs.items()}
deltas = {"body": Dg}
for sub in ("eyes", "eyebrows", "eyelashes", "teeth", "tongue"):
    P = Z["P_" + sub].astype(np.float64); o = objs[sub]
    if len(P) != len(o.data.vertices): raise SystemExit(f"{sub}: {len(P)} MHCLO vertices vs {len(o.data.vertices)} in the rig")
    deltas[sub] = to_game(P)
for k, o in objs.items(): add_all(o, deltas[k])
after = {k: (basis(o), normals(o)) for k, o in objs.items()}
for k in objs: log(f"  {k}: max {np.abs(deltas[k]).max()*1000:.1f} mm")
os.makedirs(OUT, exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, f"{MID}_game.blend"))

# ---------------------------------------------------------------- the GLB's own vertices
def read_glb(path):
    b = open(path, "rb").read(); off, js, bn = 12, None, None
    while off < len(b):
        cl, ct = struct.unpack_from("<II", b, off); off += 8
        if ct == 0x4E4F534A: js = json.loads(b[off:off + cl])
        elif ct == 0x004E4942: bn = b[off:off + cl]
        off += cl
    return js, bn
def acc(js, bn, i):
    a = js["accessors"][i]; bv = js["bufferViews"][a["bufferView"]]
    assert a["componentType"] == 5126 and "sparse" not in a
    nc = {"VEC3": 3, "VEC2": 2, "VEC4": 4}[a["type"]]
    st = bv.get("byteStride", 0) or 4 * nc
    raw = np.frombuffer(bn, np.uint8, st * (a["count"] - 1) + 4 * nc, bv.get("byteOffset", 0) + a.get("byteOffset", 0))
    idx = np.arange(a["count"])[:, None] * st + np.arange(4 * nc)[None]
    return raw[idx].copy().view(np.float32).reshape(-1, nc).astype(np.float64)
js, bn = read_glb(GLB)
# Blender (Z up) -> glTF (Y up)
def conv(p): return np.stack([p[:, 0], p[:, 2], -p[:, 1]], 1)
entries, blobs = [], []
off = 0
for k, o in objs.items():
    mi = next(i for i, m in enumerate(js["meshes"]) if m.get("name") == MESH[k])
    for pi, prim in enumerate(js["meshes"][mi]["primitives"]):
        P = acc(js, bn, prim["attributes"]["POSITION"])
        b0, n0 = before[k]; a1, n1 = after[k]
        kd = KDTree(len(b0))
        for i, p in enumerate(conv(b0)): kd.insert(p, i)
        kd.balance()
        near = np.array([kd.find(p) for p in P], dtype=object)
        j = np.array([x[1] for x in near]); dist = np.array([x[2] for x in near])
        log(f"  glb {MESH[k]}[{pi}]: {len(P)} verts, match max {dist.max()*1000:.4f} mm")
        if dist.max() > 2e-4: raise SystemExit(f"{MESH[k]}: GLB vertices do not match the rig (max {dist.max()*1000:.3f} mm)")
        dp = conv(a1[j] - b0[j]); dn = conv(n1[j] - n0[j])
        keep = np.where((np.abs(dp).max(1) > 2e-6) | (np.abs(dn).max(1) > 2e-4))[0].astype(np.uint32)
        blob = keep.tobytes() + dp[keep].astype(np.float32).tobytes() + dn[keep].astype(np.float32).tobytes()
        entries.append({"name": MESH[k], "prim": pi, "vertexCount": int(len(P)), "n": int(len(keep)), "offset": off})
        blobs.append(blob); off += len(blob)
head = json.dumps({"model": MID, "base": M["base"], "meshes": entries}).encode()
head += b" " * ((4 - len(head) % 4) % 4)
fn = os.path.join(OUT, f"{MID}_identity.bin")
with open(fn, "wb") as f:
    f.write(b"AGID"); f.write(struct.pack("<II", 1, len(head))); f.write(head)
    for b_ in blobs: f.write(b_)
log(f"wrote {fn} ({os.path.getsize(fn)//1024} KB), {sum(e['n'] for e in entries)} vertices moved")
