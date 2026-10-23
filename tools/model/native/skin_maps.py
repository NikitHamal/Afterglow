# Skin support maps for the engine (optional inputs, see docs/NATIVE_CONTENT.md "Skin support maps"):
#   <who>_skin_mask.png  1024², linear RGB on the body UV (uv0, same layout as the skin albedo):
#       R = flush (where blood shows first: cheeks, nose, ears, neck, décolleté, breasts / his chest, genitals)
#       G = translucency (exp(-thickness / 10 mm), thickness ray-cast through the mesh: ears, nose, fingers, toes)
#       B = sweat / sheen (forehead, temples, upper lip, neck, chest centre, under the breasts, lower back)
#   skin_detail_n.png    512² tileable tangent-space micro normal (pores + fine crosshatch), linear; tile ~48x on uv0
#   blender -b <stage blend with "Human" + "Human.rig"> --python skin_maps.py -- vivian|marcus
import bpy, sys, os, json, math, time
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from PIL import Image
who = sys.argv[sys.argv.index("--") + 1]
NPZ = "/root/agb/out/bakecache5.npz" if who == "vivian" else "/root/agb/male/out/bakes.npz"
LMF = "/root/agb/out/landmarks.json" if who == "vivian" else "/root/agb/male/out/landmarks.json"
OUTD = f"/root/agb/native/tex/{who}"; os.makedirs(OUTD, exist_ok=True)
t0 = time.time()
def ss(e0, e1, x):
    t = np.clip((np.asarray(x, np.float64) - e0) / (e1 - e0), 0, 1); return t * t * (3 - 2 * t)
z = np.load(NPZ); R_ = 4                                          # 4096 -> 1024
P = z["POS"][::R_, ::R_].astype(np.float64); N = z["NRM"][::R_, ::R_].astype(np.float64); C = z["COV"][::R_, ::R_]
Hm = z["MH"][::R_, ::R_] > 0.5
Lm = json.load(open(LMF)); EL, ER = np.array(Lm["EL"]), np.array(Lm["ER"]); EC = (EL + ER) / 2
NIP = [np.array(n) for n in Lm["NIP"]] if isinstance(Lm["NIP"][0], list) else [np.array(Lm["NIP"]), np.array(Lm["NIP"]) * np.array([-1, 1, 1])]
rig = bpy.data.objects["Human.rig"]; RB = rig.data.bones
def bone_head(n): return np.array(rig.matrix_world @ RB[n].head_local)
PEL = bone_head("pelvis"); SP1 = bone_head("spine_01") if "spine_01" in RB else PEL + np.array([0, 0, 0.12])
def g(c, sig, w=1.0, scale=(1, 1, 1)):
    d = (P - c) * np.array(scale); return w * np.exp(-np.sum(d * d, -1) / (2 * sig * sig))
fwd = -N[..., 1]                                                  # front of the body faces -Y
# ---------------------------------------------------------------- R: flush
fl = np.zeros(P.shape[:2])
for e, sx in ((EL, 1), (ER, -1)):
    fl = np.maximum(fl, g(e + np.array([sx * 0.010, -0.004, -0.034]), 0.017, 1.0))           # cheeks
fl = np.maximum(fl, g(EC + np.array([0, -0.030, -0.036]), 0.010, 0.55))                         # nose tip
chin_z = Lm.get("CHIN_Z", float(EC[2] - 0.115))
nz = (NIP[0][2] + NIP[1][2]) / 2
neck = ss(nz + 0.02, nz + 0.10, P[..., 2]) * ss(chin_z + 0.01, chin_z - 0.03, P[..., 2]) * ss(0.0, 0.4, fwd)
fl = np.maximum(fl, neck * np.exp(-P[..., 0] ** 2 / (2 * 0.085 ** 2)) * 0.6)                    # neck + décolleté
for n_ in NIP: fl = np.maximum(fl, g(n_ + np.array([0, 0.01, 0.03]), 0.055, 0.32 if who == "vivian" else 0.22))
crotch = PEL + np.array([0, -0.06, -0.10])
fl = np.maximum(fl, g(crotch, 0.05, 0.45))                                                       # genital flush
# ---------------------------------------------------------------- G: translucency from ray-cast thickness
body = bpy.data.objects["Human"]
for kb in (body.data.shape_keys.key_blocks if body.data.shape_keys else []): kb.value = 0
for m in body.modifiers: m.show_viewport = False
deps = bpy.context.evaluated_depsgraph_get(); me = body.evaluated_get(deps).to_mesh()
V = [tuple(v.co) for v in me.vertices]; F = [tuple(p.vertices) for p in me.polygons]
bvh = BVHTree.FromPolygons(V, F); body.evaluated_get(deps).to_mesh_clear()
th = np.full(P.shape[:2], 0.08)
idx = np.argwhere(C)
for (i, j) in idx:
    d = Vector(-N[i, j]); o = Vector(P[i, j]) + d * 0.0004; acc = 0.0004
    for _ in range(4):                                       # skip inner/self hits (faces facing the ray): only an exit counts
        hit = bvh.ray_cast(o, d, 0.08 - acc)
        if hit[0] is None: break
        acc += hit[3]
        if hit[1].dot(d) > 0.2 and acc > 0.0015: th[i, j] = acc; break
        o = hit[0] + d * 0.0003; acc += 0.0003
tr = np.exp(-th / 0.010) * C
fl = np.maximum(fl, tr * Hm * 0.6)                                                               # ears flush too
# ---------------------------------------------------------------- B: sweat / sheen
sw = np.zeros(P.shape[:2])
sw = np.maximum(sw, ss(EC[2] + 0.015, EC[2] + 0.035, P[..., 2]) * ss(EC[2] + 0.085, EC[2] + 0.065, P[..., 2]) * ss(0.2, 0.6, fwd) * Hm * 0.85)
for e, sx in ((EL, 1), (ER, -1)): sw = np.maximum(sw, g(e + np.array([sx * 0.045, 0.02, 0.015]), 0.014, 0.5))     # temples
mouth = np.array(Lm["MOUTH"]) if "MOUTH" in Lm else EC + np.array([0, -0.02, -0.065])
sw = np.maximum(sw, g(mouth + np.array([0, -0.004, 0.013]), 0.008, 0.6))                         # upper lip
sw = np.maximum(sw, neck * 0.55)
sw = np.maximum(sw, g(np.array([0, (NIP[0][1] + NIP[1][1]) / 2 + 0.03, nz + 0.01]), 0.045, 0.85, (1.4, 1, 0.8)))   # sternum
for n_ in NIP: sw = np.maximum(sw, g(n_ + np.array([0, 0.035, -0.075]), 0.035, 0.6 if who == "vivian" else 0.35))  # under the breasts
back = ss(-0.1, -0.5, fwd) * ss(PEL[2] + 0.02, PEL[2] + 0.08, P[..., 2]) * ss(PEL[2] + 0.30, PEL[2] + 0.20, P[..., 2])
sw = np.maximum(sw, back * np.exp(-P[..., 0] ** 2 / (2 * 0.07 ** 2)) * 0.6)                      # lower back
img = np.stack([np.clip(fl, 0, 1) * C, np.clip(tr, 0, 1), np.clip(sw, 0, 1) * C], -1)
# grow the islands a few texels so bilinear sampling at seams never reads black
for _ in range(4):
    m = img.max(-1) > 0
    acc = np.zeros_like(img); cnt = np.zeros(img.shape[:2])
    for di, dj in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        sh = np.roll(np.roll(img, di, 0), dj, 1); sm = np.roll(np.roll(m, di, 0), dj, 1)
        acc += sh * sm[..., None]; cnt += sm
    fill = (~m) & (cnt > 0); img[fill] = acc[fill] / cnt[fill][:, None]
Image.fromarray(np.flipud(np.clip(img * 255 + 0.5, 0, 255).astype(np.uint8)), "RGB").save(f"{OUTD}/{who}_skin_mask.png", optimize=True)
print(f"[skin] {who}: mask written ({time.time()-t0:.0f}s), translucent texels {(tr > 0.3).sum()}", flush=True)
# ---------------------------------------------------------------- shared tileable detail normal
dn = "/root/agb/native/tex/skin_detail_n.png"
if not os.path.exists(dn):
    rng = np.random.default_rng(3); S = 512; h = np.zeros((S, S))
    yy, xx = np.mgrid[0:S, 0:S]
    for _ in range(2600):                                        # pores: small periodic dimples
        cx, cy, r = rng.uniform(0, S), rng.uniform(0, S), rng.uniform(1.2, 2.6)
        dx = (xx - cx + S / 2) % S - S / 2; dy = (yy - cy + S / 2) % S - S / 2
        h -= rng.uniform(0.5, 1.0) * np.exp(-(dx * dx + dy * dy) / (2 * r * r))
    for k in range(10):                                          # fine crosshatch (periodic waves)
        a = rng.uniform(0, np.pi); f = rng.integers(20, 60)
        ph = rng.uniform(0, 2 * np.pi); fx, fy = round(f * np.cos(a)), round(f * np.sin(a))
        h += 0.10 * np.sin(2 * np.pi * (fx * xx + fy * yy) / S + ph)
    F_ = np.fft.rfft2(rng.normal(0, 1, (S, S))); ky = np.fft.fftfreq(S)[:, None]; kx = np.fft.rfftfreq(S)[None, :]
    h += 0.35 * np.fft.irfft2(F_ * np.exp(-(kx ** 2 + ky ** 2) / (2 * 0.04 ** 2)), s=(S, S))
    h = (h - h.mean()) / h.std()
    gx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 0.5; gy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * 0.5
    st = 0.35; n = np.stack([-gx * st, gy * st, np.ones_like(h)], -1); n /= np.linalg.norm(n, axis=-1, keepdims=True)
    Image.fromarray(np.clip((n * 0.5 + 0.5) * 255 + 0.5, 0, 255).astype(np.uint8), "RGB").save(dn, optimize=True)
    print("[skin] detail normal written", flush=True)
