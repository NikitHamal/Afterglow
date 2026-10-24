# More expressive faces, step 2 of 2: the expression morphs of expr_units.py spliced into the shipped GLB as new morph
# targets (sparse POSITION + NORMAL deltas on the body and every face part that moves; nothing else in the file changes).
#   blender -b <base>_game.blend --python tools/model/native/expr_apply.py -- <base> <expr.npz> <in.glb> <out.glb>
# The MakeHuman -> game -> GLB vertex correspondence is the identity packs' (similarity fit on the MakeHuman body
# vertices, then nearest rest vertex within 0.2 mm) and is checked before anything is written. Existing targets with
# the same name are left alone (the run refuses to duplicate a name).
import bpy, sys, os, json, struct
import numpy as np
from mathutils.kdtree import KDTree

argv = sys.argv[sys.argv.index("--") + 1:]
BASE_ID, NPZ, GLB_IN, GLB_OUT = argv[:4]
HERE = os.path.dirname(os.path.abspath(__file__))
R = json.load(open(os.path.join(HERE, "..", "identity", "recipes.json")))
MESH = R["bases"][BASE_ID]["meshes"]
Z = np.load(NPZ)
NAMES = [str(n) for n in Z["names"]]
NBODY = 13380
def log(*a): print("[expr_apply]", *a, flush=True)

def basis(o):
    me = o.data
    if me.shape_keys and "Basis" in me.shape_keys.key_blocks:
        kb = me.shape_keys.key_blocks["Basis"]; a = np.empty(len(kb.data) * 3, np.float32); kb.data.foreach_get("co", a)
    else:
        a = np.empty(len(me.vertices) * 3, np.float32); me.vertices.foreach_get("co", a)
    return a.reshape(-1, 3).astype(np.float64)
def normals_at(o, P):
    """vertex normals of the mesh with its vertices at P (a temporary copy: the rig is not changed)"""
    me = o.data.copy()
    me.vertices.foreach_set("co", P.astype(np.float32).ravel()); me.update()
    a = np.empty(len(me.vertices) * 3, np.float32); me.vertices.foreach_get("normal", a)
    bpy.data.meshes.remove(me)
    return a.reshape(-1, 3).astype(np.float64)
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
moving = np.where(np.abs(Z["D_" + NAMES[0]][:n]).max(1) > 1e-5)[0] if NAMES else np.arange(n)
log(f"MakeHuman -> game: scale {s:.4f}, residual median {np.median(res)*1000:.3f} mm, face max {res[moving].max()*1000:.3f} mm")
if np.percentile(res[moving], 99) > 0.5e-3: raise SystemExit("vertex correspondence broken: refusing to write")
def to_game(d): return (s * (Rm @ d.T)).T

SUBS = {"body": None, "eyebrows": "eyebrows", "eyelashes": "eyelashes", "teeth": "teeth", "tongue": "tongue"}
objs = {k: bpy.data.objects[MESH[k]] for k in SUBS}
rest = {k: basis(o) for k, o in objs.items()}
rest_n = {k: normals_at(o, rest[k]) for k, o in objs.items()}
deltas = {}   # (part, name) -> (dp, dn) on the rig's vertices
for name in NAMES:
    for k, o in objs.items():
        if k == "body":
            d = np.zeros_like(rest[k]); d[:n] = to_game(Z["D_" + name][:n].astype(np.float64))
        else:
            P = Z[f"P_{k}_{name}"].astype(np.float64)
            if len(P) != len(rest[k]): raise SystemExit(f"{k}: {len(P)} MHCLO vertices vs {len(rest[k])} in the rig")
            d = to_game(P)
        if np.abs(d).max() < 1e-6: continue
        deltas[(k, name)] = (d, normals_at(o, rest[k] + d) - rest_n[k])
    log(f"  {name}: " + ", ".join(f"{k} {np.abs(deltas[(k, name)][0]).max()*1000:.1f} mm" for k in objs if (k, name) in deltas))

# ---------------------------------------------------------------- the GLB
def read_glb(path):
    b = open(path, "rb").read(); off, js, bn = 12, None, None
    while off < len(b):
        cl, ct = struct.unpack_from("<II", b, off); off += 8
        if ct == 0x4E4F534A: js = json.loads(b[off:off + cl])
        elif ct == 0x004E4942: bn = bytearray(b[off:off + cl])
        off += cl
    return js, bn
def write_glb(path, js, bn):
    while len(bn) % 4: bn.append(0)
    js["buffers"][0]["byteLength"] = len(bn)
    j = json.dumps(js, separators=(",", ":"), ensure_ascii=False).encode()
    j += b" " * ((4 - len(j) % 4) % 4)
    with open(path, "wb") as f:
        f.write(struct.pack("<III", 0x46546C67, 2, 12 + 8 + len(j) + 8 + len(bn)))
        f.write(struct.pack("<II", len(j), 0x4E4F534A)); f.write(j)
        f.write(struct.pack("<II", len(bn), 0x004E4942)); f.write(bytes(bn))
def acc(js, bn, i):
    a = js["accessors"][i]; bv = js["bufferViews"][a["bufferView"]]
    nc = {"VEC3": 3, "VEC2": 2, "VEC4": 4}[a["type"]]
    st = bv.get("byteStride", 0) or 4 * nc
    raw = np.frombuffer(bytes(bn), np.uint8, st * (a["count"] - 1) + 4 * nc, bv.get("byteOffset", 0) + a.get("byteOffset", 0))
    idx = np.arange(a["count"])[:, None] * st + np.arange(4 * nc)[None]
    return raw[idx].copy().view(np.float32).reshape(-1, nc).astype(np.float64)
def view(js, bn, data):
    while len(bn) % 4: bn.append(0)
    o = len(bn); bn.extend(data)
    js["bufferViews"].append({"buffer": 0, "byteOffset": o, "byteLength": len(data)})
    return len(js["bufferViews"]) - 1
def sparse_acc(js, bn, count, idx, vals, minmax):
    """a VEC3 float accessor of `count` zeros with `vals` at `idx` (at least one entry, as the spec asks)"""
    if len(idx) == 0: idx, vals = np.zeros(1, np.uint32), np.zeros((1, 3))
    a = {"componentType": 5126, "count": int(count), "type": "VEC3",
         "sparse": {"count": int(len(idx)), "indices": {"bufferView": view(js, bn, idx.astype(np.uint32).tobytes()), "componentType": 5125},
                    "values": {"bufferView": view(js, bn, vals.astype(np.float32).tobytes())}}}
    if minmax:
        full = np.vstack([vals, np.zeros((1, 3))])
        a["min"] = [float(x) for x in full.min(0)]; a["max"] = [float(x) for x in full.max(0)]
    js["accessors"].append(a)
    return len(js["accessors"]) - 1

js, bn = read_glb(GLB_IN)
def conv(p): return np.stack([p[:, 0], p[:, 2], -p[:, 1]], 1)   # Blender (Z up) -> glTF (Y up)
added = 0
for k, o in objs.items():
    names = [nm for nm in NAMES if (k, nm) in deltas]
    if not names: continue
    mi = next(i for i, m in enumerate(js["meshes"]) if m.get("name") == MESH[k])
    mesh = js["meshes"][mi]
    have = (mesh.get("extras") or {}).get("targetNames", [])
    dup = [nm for nm in names if nm in have]
    if dup: raise SystemExit(f"{MESH[k]} already has {dup}: splice into the GLB without them")
    kd = KDTree(len(rest[k]))
    for i, p in enumerate(conv(rest[k])): kd.insert(p, i)
    kd.balance()
    for pi, prim in enumerate(mesh["primitives"]):
        P = acc(js, bn, prim["attributes"]["POSITION"])
        near = [kd.find(p) for p in P]
        j = np.array([x[1] for x in near]); dist = np.array([x[2] for x in near])
        on = dist < 2e-4
        if pi == 0 and not on.all(): raise SystemExit(f"{MESH[k]}: GLB vertices do not match the rig (max {dist.max()*1000:.3f} mm)")
        log(f"  glb {MESH[k]}[{pi}]: {len(P)} verts, {int(on.sum())} on the rig (match max {dist[on].max()*1000 if on.any() else 0:.4f} mm)")
        prim.setdefault("targets", [])
        for nm in names:
            dp, dn = deltas[(k, nm)]
            DP = np.where(on[:, None], conv(dp[j]), 0.0); DN = np.where(on[:, None], conv(dn[j]), 0.0)
            keep = np.where((np.abs(DP).max(1) > 2e-6) | (np.abs(DN).max(1) > 2e-4))[0]
            prim["targets"].append({"POSITION": sparse_acc(js, bn, len(P), keep, DP[keep], True),
                                    "NORMAL": sparse_acc(js, bn, len(P), keep, DN[keep], False)})
            added += 1
    mesh.setdefault("extras", {}).setdefault("targetNames", list(have))
    mesh["extras"]["targetNames"] = list(have) + names
    if "weights" in mesh: mesh["weights"] = list(mesh["weights"]) + [0.0] * len(names)
    log(f"  {MESH[k]}: + {names}")
write_glb(GLB_OUT, js, bn)
log(f"wrote {GLB_OUT} ({os.path.getsize(GLB_OUT)//1024} KB, was {os.path.getsize(GLB_IN)//1024} KB), {added} primitive targets")
