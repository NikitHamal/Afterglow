# Vivian hair: an elegant low chignon. Hair is swept back from a soft side part over an opaque scalp cap in three
# conforming card layers, wound into a coiled bun at the nape; two face-framing tendrils and two nape wisps are the
# only loose hair (the only physics chains). Everything else rides the head bone, so it can never clip the body.
#   blender -b out/vivian_s4.blend --python hair_updo.py      -> out/vivian_s5.blend, out/hair_chains.json, tex/vivian_hair*.png
# Texture convention (see docs/NATIVE_CONTENT.md): V runs root (0) -> tip (1) along every strand, U across;
# RGB = neutral strand luminance (dark roots, lighter tips), A = coverage. Colour = baseColorFactor (espresso) x tint.
import bpy, os, math, json, time, shlex
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from PIL import Image

OUT = "/root/agb/out"; TEX = OUT + "/tex"
HAIR_HEX = "#3a2117"                      # authored espresso (engine colour presets multiply it)
def prog(*a): os.system("python3 /root/agb/progress.py " + " ".join(shlex.quote(str(x)) for x in a) + " >/dev/null 2>&1")
def log(*a): print("[hair]", *a, flush=True); prog("log", "[C] hair: " + " ".join(str(x) for x in a))
t0 = time.time(); rng = np.random.default_rng(11)
Lm = json.load(open(OUT + "/landmarks.json")); EL, ER = np.array(Lm["EL"]), np.array(Lm["ER"])
bones = {k: (np.array(v[0]), np.array(v[1])) for k, v in Lm["bones"].items()}
EC = (EL + ER) / 2
body = bpy.data.objects["Human"]; rig = bpy.data.objects["Human.rig"]
EX, EY, EZ = np.eye(3)
def ss(e0, e1, x):
    t = np.clip((np.asarray(x, np.float64) - e0) / (e1 - e0), 0, 1); return t * t * (3 - 2 * t)
def nrm(v):
    n = np.linalg.norm(v, axis=-1, keepdims=True); return v / np.maximum(n, 1e-9)

# ------------------------------------------------------------------ body geometry in rest pose, expressions off
for kb in (body.data.shape_keys.key_blocks if body.data.shape_keys else []): kb.value = 0
mods = [(m, m.show_viewport) for m in body.modifiers]
for m, _ in mods: m.show_viewport = False
deps = bpy.context.evaluated_depsgraph_get()
me = body.evaluated_get(deps).to_mesh()
V = np.empty(len(me.vertices) * 3, np.float32); me.vertices.foreach_get("co", V); V = V.reshape(-1, 3).astype(np.float64)
body.evaluated_get(deps).to_mesh_clear()
for m, s in mods: m.show_viewport = s
def vgw(name):
    w = np.zeros(len(body.data.vertices), np.float32)
    if name not in body.vertex_groups: return w
    gi = body.vertex_groups[name].index
    for v in body.data.vertices:
        for g in v.groups:
            if g.group == gi: w[v.index] = g.weight
    return w
wb, wh, we = vgw("body"), vgw("head"), vgw("ears")
polys = [tuple(p.vertices) for p in body.data.polygons if all(wb[i] > 0.5 for i in p.vertices)]
bvh = BVHTree.FromPolygons([tuple(v) for v in V], polys)
Ch = np.array([0.0, EC[1] + 0.075, EC[2] + 0.012])                 # head centre
SHZ = bones["upperarm_l"][0][2]
headv = V[(wh > 0.5) & (wb > 0.5)]
CHIN_Z = headv[(np.abs(headv[:, 0]) < 0.015) & (headv[:, 1] < EC[1])][:, 2].min()
EARC = [V[(we > 0.5) & (np.sign(V[:, 0]) == s)].mean(0) for s in (1, -1)]
TOP_Z = headv[:, 2].max()
log(f"head centre {Ch.round(3)}, chin z {CHIN_Z:.3f}, top z {TOP_Z:.3f}")
def near(p):
    loc, nn, _, d = bvh.find_nearest(tuple(p)); return np.array(loc), np.array(nn), d

def phi_of(p): return np.arctan2(p[..., 0] - Ch[0], -(p[..., 1] - Ch[1]))       # 0 = front, +-pi = back
HL_A = [0.0, 0.35, 0.70, 1.00, 1.25, 1.45, 1.75, 2.05, 2.50, math.pi]
HL_Z = [0.070, 0.064, 0.048, 0.032, 0.022, 0.016, 0.004, -0.045, -0.078, -0.088]
def hairline_z(p): return EC[2] + np.interp(np.abs(phi_of(p)), HL_A, HL_Z)
EAR_R = 0.036                                                       # hair routes around a disc around each ear
def ear_dist(p): return np.minimum(np.linalg.norm(p - EARC[0], axis=-1), np.linalg.norm(p - EARC[1], axis=-1))
def on_scalp(p): return (p[..., 2] > hairline_z(p)) & (ear_dist(p) > EAR_R + 0.004)

# ------------------------------------------------------------------ the bun: a coiled ellipsoid low at the back of the head
AX = nrm(np.array([0.0, math.cos(math.radians(24)), -math.sin(math.radians(24))]))   # bun faces back, a little down
hit = bvh.ray_cast(Vector(tuple(Ch + AX * 0.02)), Vector(tuple(AX)), 0.3)
back_pt = np.array(hit[0]) if hit[0] is not None else Ch + AX * 0.095
E1 = nrm(EX - AX * (EX @ AX)); E2 = nrm(np.cross(AX, E1))
if E2[2] < 0: E2 = -E2
BUN_C = back_pt + AX * 0.019
BUN_A = np.array([0.038, 0.029, 0.022])                            # half-width, half-height, protrusion
def bun_pt(lam, phi, k=1.0):
    a = BUN_A * k
    return BUN_C + E1 * (a[0] * np.cos(lam) * np.cos(phi)) + E2 * (a[1] * np.cos(lam) * np.sin(phi)) + AX * (a[2] * np.sin(lam))
def bun_n(lam, phi, k=1.0):
    a = BUN_A * k
    g = E1 * (np.cos(lam) * np.cos(phi) / a[0]) + E2 * (np.cos(lam) * np.sin(phi) / a[1]) + AX * (np.sin(lam) / a[2])
    return nrm(g)
def in_bun(p, pad=0.0):
    d = p - BUN_C; q = np.array([d @ E1, d @ E2, d @ AX]) / (BUN_A + pad)
    return float(q @ q) < 1.0
log(f"bun centre {BUN_C.round(3)}, axis {AX.round(2)}")

# ------------------------------------------------------------------ roots: area-weighted samples + poisson thinning
sv = (wh > 0.5) & (wb > 0.5) & on_scalp(V) & (we < 0.2)
tris = []
for p in body.data.polygons:
    vs = list(p.vertices)
    if all(sv[i] for i in vs):
        tris.append((vs[0], vs[1], vs[2]))
        if len(vs) == 4: tris.append((vs[0], vs[2], vs[3]))
tris = np.array(tris); A, B, C = V[tris[:, 0]], V[tris[:, 1]], V[tris[:, 2]]
area = 0.5 * np.linalg.norm(np.cross(B - A, C - A), axis=1)
NC_ = 60000
pick = rng.choice(len(tris), size=NC_, p=area / area.sum())
r1, r2 = np.sqrt(rng.random(NC_)), rng.random(NC_)
cand = A[pick] * (1 - r1)[:, None] + B[pick] * (r1 * (1 - r2))[:, None] + C[pick] * (r1 * r2)[:, None]
def poisson(pts, r):
    keep, grid = [], {}
    for i, p in enumerate(pts):
        key = tuple((p // r).astype(int)); ok = True
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                for dz in (-1, 0, 1):
                    for j in grid.get((key[0] + dx, key[1] + dy, key[2] + dz), ()):
                        if np.sum((pts[j] - p) ** 2) < r * r: ok = False; break
                    if not ok: break
                if not ok: break
            if not ok: break
        if ok: keep.append(i); grid.setdefault(key, []).append(i)
    return pts[keep]
ROOTS = poisson(cand, 0.0046)
ROOTS = ROOTS[[not in_bun(p, 0.004) for p in ROOTS]]
log(f"scalp: {len(tris)} tris, {area.sum()*1e4:.0f} cm2, {len(ROOTS)} roots")

# ------------------------------------------------------------------ swept-back strands that hug the skull into the bun
PART_X = 0.0                                                        # no part: pulled straight back into the bun
def tangent(v, n): v = v - n * (v @ n); return v / max(1e-9, np.linalg.norm(v))
def around_ears(p, n):
    for c in EARC:
        d = p - c; d = d - n * (d @ n); L = np.linalg.norm(d)
        if L < EAR_R: p = p + d / max(L, 1e-6) * (EAR_R - L)
    return p
BA = nrm(BUN_C - Ch)                                               # flow = meridians of the head toward the bun pole
CROWN = Ch + np.array([0.0, 0.055, 0.105])
def flow(p, n):
    q = nrm(p - Ch); t = BA - q * (q @ BA)
    w = float(ss(-0.45, -0.80, q @ BA))                             # near the anti-pole (front hairline): over the crown first
    t = t * (1 - w) + nrm(CROWN - p) * w
    t = tangent(t, n)
    for c in EARC:                                                  # slide around the ears (above them), never into them
        d = p - c; d = d - n * (d @ n); L = np.linalg.norm(d)
        if L < 0.075:
            dh = d / max(L, 1e-6); b = float(ss(0.075, 0.045, L))
            into = min(0.0, t @ dh)
            t = tangent(t - dh * into * b + dh * 0.25 * b, n)
    return t
def on_face(p): return p[1] < EC[1] + 0.03 and p[2] < hairline_z(p) - 0.003
def grow(root, off, seg=0.005, max_len=0.34):
    sgn = 1.0 if root[0] >= PART_X else -1.0
    front = root[1] < Ch[1] - 0.02 and root[2] > EC[2] + 0.04 and abs(root[0] - PART_X) < 0.045
    crown = ss(EC[2] + 0.07, TOP_Z - 0.01, root[2])
    p = root.copy(); d = None; pts = []; s = 0.0
    while s < max_len:
        loc, n, _ = near(p)
        o = off + 0.0035 * crown * ss(0.0, 0.05, s) * ss(0.22, 0.10, s)  # a little lift over the crown
        p = loc + n * o; p = around_ears(p, n)
        if (in_bun(p, 0.002) and pts) or on_face(p): break
        pts.append(p.copy())
        f = flow(p, n)
        if front: f = tangent(f + EZ * 0.4 * float(ss(0.03, 0.0, s)), n)       # sleek: straight up off the forehead, then back
        d = f if d is None else tangent(d * 0.6 + f * 0.4, n)
        p = p + d * seg; s += seg
    return np.array(pts)
def resample(P, n):
    seg = np.linalg.norm(np.diff(P, axis=0), axis=1); cl = np.concatenate([[0], np.cumsum(seg)])
    t = np.linspace(0, cl[-1], n); return np.stack([np.interp(t, cl, P[:, k]) for k in range(3)], 1), cl[-1]
NSEG = 10
layers = []
for li, (off, frac, width, cols) in enumerate([(0.0024, 1.0, 0.0105, (0, 1, 2, 3)), (0.0048, 0.9, 0.0090, (0, 1, 2, 3)),
                                              (0.0066, 0.6, 0.0080, (0, 1, 2, 3, 4, 5))]):
    idx = np.where(rng.random(len(ROOTS)) < frac)[0]
    S, keep = [], []
    for i in idx:
        r = ROOTS[i] + (rng.normal(0, 0.0012, 3) if li else 0)
        P = grow(r, off)
        if len(P) < 4: continue
        P, ln = resample(P, NSEG + 1)
        if ln < 0.02: continue
        S.append(P); keep.append(i)
    layers.append(dict(S=np.array(S), roots=ROOTS[keep], width=width, cols=cols, off=off))
    log(f"layer {li}: {len(S)} strands")

# clumps: neighbouring strands pull together toward the bun so the sweep reads as locks, not a painted shell
uN = np.linspace(0, 1, NSEG + 1)
for L_ in layers:
    S = L_["S"]; R_ = L_["roots"]; K = max(8, len(S) // 9)
    cent = R_[rng.choice(len(R_), K, replace=False)]
    for _ in range(10):
        lab = np.argmin(((R_[:, None] - cent[None]) ** 2).sum(-1), 1)
        cent = np.array([R_[lab == k].mean(0) if (lab == k).any() else cent[k] for k in range(K)])
    guides = np.array([S[lab == k].mean(0) if (lab == k).any() else np.zeros((NSEG + 1, 3)) for k in range(K)])
    S = S + (guides[lab] - S) * (0.18 * ss(0.25, 0.9, uN))[None, :, None]
    for si in range(len(S)):                                        # re-seat on the skull after clumping
        for k in range(NSEG + 1):
            loc, n, _ = near(S[si, k]); h = (S[si, k] - loc) @ n
            if h < L_["off"] * 0.8: S[si, k] = loc + n * L_["off"]
    L_["S"] = S

# ------------------------------------------------------------------ bun strands: loxodromic coils on 3 shells
BUN_S = []
for shell, (k, n_) in enumerate([(1.00, 150), (1.10, 120), (1.20, 70)]):
    for _ in range(n_):
        phi0 = rng.uniform(0, 2 * np.pi); turns = rng.uniform(0.9, 1.4) * (1 if shell != 1 else -1)
        lam0, lam1 = rng.uniform(-0.55, -0.15), rng.uniform(0.95, 1.40)
        t = np.linspace(0, 1, 13)
        lam = lam0 + (lam1 - lam0) * t; ph = phi0 + 2 * np.pi * turns * t
        P = np.array([bun_pt(a, b, k) for a, b in zip(lam, ph)]); Nn = np.array([bun_n(a, b, k) for a, b in zip(lam, ph)])
        BUN_S.append((P, Nn, 0.0105 + 0.002 * rng.random()))
log(f"bun: {len(BUN_S)} coiled strands")

# ------------------------------------------------------------------ loose hair: two face-framing tendrils, two nape wisps
def face_half(z): return 0.062 + 0.010 * ss(EC[2] - 0.02, CHIN_Z, z)
LOOSE = []
for sx in (1, -1):                                                  # tendrils: temple hairline -> jaw, outside the cheek
    ph = sx * 1.22
    cands_ = ROOTS[(np.abs(phi_of(ROOTS) - ph) < 0.12)]
    r0 = cands_[np.argmin(np.abs(cands_[:, 2] - hairline_z(cands_)))]
    L = 0.135; NT = 16; pts = []
    for i in range(NT + 1):
        s = L * i / NT; z = r0[2] - s
        x = sx * max(abs(r0[0]) + 0.004 + 0.010 * s / L, face_half(z) + 0.012) + sx * 0.004 * math.sin(2 * math.pi * s / 0.07)
        y = r0[1] - 0.008 * math.sin(math.pi * s / L) + 0.010 * s / L
        p = np.array([x, y, z]); loc, n, _ = near(p)
        if (p - loc) @ n < 0.006: p = loc + n * 0.006
        pts.append(p)
    LOOSE.append(dict(G=np.array(pts), cards=9, width=0.0080, cols=(4, 5, 6, 7), name="tendril"))
for sx in (1, -1):                                                  # nape wisps under the bun, along the neck
    cands_ = ROOTS[(np.abs(np.abs(phi_of(ROOTS)) - 2.75) < 0.12) & (np.sign(ROOTS[:, 0]) == sx)]
    r0 = cands_[np.argmin(cands_[:, 2])]
    L = 0.075; NT = 12; pts = []
    for i in range(NT + 1):
        s = L * i / NT
        p = r0 + np.array([sx * 0.006 * s / L, 0.004, -s]) + np.array([sx * 0.003 * math.sin(2 * math.pi * s / 0.05), 0, 0])
        loc, n, _ = near(p)
        if (p - loc) @ n < 0.005: p = loc + n * 0.005
        pts.append(p)
    LOOSE.append(dict(G=np.array(pts), cards=5, width=0.0052, cols=(6, 7), name="wisp"))

CH_U = np.array([0.22, 0.42, 0.62, 0.82, 1.0])
CHAINS = []
for lo in LOOSE:
    G = lo["G"]; uG = np.linspace(0, 1, len(G))
    CHAINS.append(np.stack([np.interp(CH_U, uG, G[:, d]) for d in range(3)], 1))
json.dump([J.tolist() for J in CHAINS], open(OUT + "/hair_chains.json", "w"))
json.dump(dict(kinds=[lo["name"] for lo in LOOSE], bun=dict(center=BUN_C.tolist(), axis=AX.tolist(), semi=BUN_A.tolist())),
          open(OUT + "/hair_meta.json", "w"))

# ------------------------------------------------------------------ strand atlas: 8 columns (0-3 dense sheets, 4-5 locks, 6-7 wisps)
TH, TW, NCOL = 2048, 2048, 8; CW = TW // NCOL
lum = np.zeros((TH, TW), np.float32); alp = np.zeros((TH, TW), np.float32)
v = np.linspace(0, 1, TH)
def strand_layer(c, n, base, lum_rng, wav_amp, w_rng, end_rng):
    sl = slice(c * CW, (c + 1) * CW)
    if base > 0:
        u = np.linspace(0, 1, CW)
        b = (ss(0.08, 0.40, u) * ss(0.92, 0.60, u))[None, :] * (base * ss(0.0, 0.03, v) * ss(1.0, 0.86, v))[:, None]
        alp[:, sl] = np.maximum(alp[:, sl], b); lum[:, sl] = np.where(b > 0, 0.62, lum[:, sl])
    for _ in range(n):
        x0 = CW / 2 + rng.normal(0, CW * 0.22); x0 = float(np.clip(x0, 8, CW - 8)); wpx = rng.uniform(*w_rng); end = rng.uniform(*end_rng)
        L0 = rng.uniform(*lum_rng); fq = rng.uniform(0.6, 2.2); ph = rng.uniform(0, 6.3); drift = rng.uniform(-5, 5)
        xs = x0 + wav_amp * rng.uniform(0.3, 1.0) * np.sin(2 * np.pi * fq * v + ph) + drift * v
        a_len = ss(0.0, 0.015, v) * ss(end, end - 0.16, v) * rng.uniform(0.85, 1.0)
        shade = L0 * (0.55 + 0.45 * ss(0.0, 0.30, v)) * (1.0 + 0.16 * ss(0.55, 1.0, v))          # dark roots, lighter tips
        for dxp in range(-2, 3):
            xi = np.clip(np.round(xs).astype(int) + dxp, 0, CW - 1)
            cov = np.clip(1 - np.abs(xi - xs) / wpx, 0, 1) * a_len
            gx = c * CW + xi; rows = np.arange(TH)
            a_old = alp[rows, gx]; a_new = a_old + cov * (1 - a_old)
            lum[rows, gx] = np.where(a_new > 1e-4, (lum[rows, gx] * a_old + shade * cov * (1 - a_old)) / np.maximum(a_new, 1e-4), lum[rows, gx])
            alp[rows, gx] = a_new
for c in range(4): strand_layer(c, 340, 0.55, (0.50, 1.0), 1.5, (0.9, 1.5), (0.82, 1.0))
for c in (4, 5): strand_layer(c, 190, 0.30, (0.55, 1.05), 2.5, (0.9, 1.4), (0.70, 1.0))
for c in (6, 7): strand_layer(c, 70, 0.0, (0.65, 1.05), 4.0, (1.0, 1.6), (0.62, 1.0))
a8 = np.clip(alp * 1.1, 0, 1); a8 = np.where(a8 < 0.08, 0.0, a8)                     # clean alpha: no faint haze
rgb = np.stack([lum, lum * 0.93, lum * 0.86], -1)
img = np.concatenate([rgb, a8[..., None]], -1)
Image.fromarray(np.flipud(np.clip(img * 255 + 0.5, 0, 255).astype(np.uint8)), "RGBA").save(TEX + "/vivian_hair.png", optimize=True)
# cap: tileable opaque strands (periodic in U and V), sits under the cards and hides the scalp
CT = 512; cl = np.full((CT, CT), 0.5, np.float32); vv = np.arange(CT) / CT
for _ in range(900):
    x0 = rng.uniform(0, CT); L0 = rng.uniform(0.40, 0.85); fq = rng.integers(1, 3); ph = rng.uniform(0, 6.3)
    xs = (x0 + 2.0 * np.sin(2 * np.pi * fq * vv + ph)) % CT
    for dxp in (-1, 0, 1):
        xi = (np.round(xs).astype(int) + dxp) % CT; cov = np.clip(1 - np.abs(((xi - xs + CT / 2) % CT) - CT / 2) / 1.2, 0, 1) * 0.7
        cl[np.arange(CT), xi] = cl[np.arange(CT), xi] * (1 - cov) + L0 * cov
cap_rgb = np.stack([cl, cl * 0.93, cl * 0.86], -1)
Image.fromarray(np.clip(cap_rgb * 255 + 0.5, 0, 255).astype(np.uint8), "RGB").save(TEX + "/vivian_hair_cap.png", optimize=True)
log("strand atlas + cap texture painted")

# ------------------------------------------------------------------ meshes
SEGS = "abcd"
def chain_weights(ch, u):
    out = {}; head_w = float(ss(0.27, 0.12, u))
    if head_w > 0: out["head"] = head_w
    rest = 1.0 - head_w
    if rest <= 0: return out
    f = np.clip((u - CH_U[0]) / (CH_U[-1] - CH_U[0]), 0, 0.9999) * 4.0; j = int(f); t = f - j
    out[f"hair_{ch}_{SEGS[j]}"] = rest * (1 - t)
    if t > 0: out[f"hair_{ch}_{SEGS[min(j + 1, 3)]}"] = out.get(f"hair_{ch}_{SEGS[min(j + 1, 3)]}", 0) + rest * t
    return out
verts, faces, uvs, vw, fmat = [], [], [], [], []
def card(P, Bv, w, col, wfun, mat=0, twist_sd=0.0, outv=None):
    nseg = len(P) - 1; uu = np.linspace(0, 1, nseg + 1)
    if twist_sd > 0 and outv is not None:
        tw = np.radians(rng.normal(0, twist_sd)) + np.radians(rng.normal(0, twist_sd * 0.5)) * uu
        Bv = nrm(Bv * np.cos(tw)[:, None] + outv * np.sin(tw)[:, None])
    u0, u1 = (col + 0.03) / NCOL, (col + 0.97) / NCOL
    base = len(verts)
    for k in range(nseg + 1):
        verts.extend([P[k] - Bv[k] * w[k] / 2, P[k] + Bv[k] * w[k] / 2]); uvs.extend([(u0, uu[k]), (u1, uu[k])])
        wk = wfun(uu[k]); vw.extend([wk, wk])
    for k in range(nseg):
        a = base + 2 * k; faces.append((a, a + 1, a + 3, a + 2)); fmat.append(mat)
HEAD = lambda u: {"head": 1.0}
for L_ in layers:                                                   # swept sheets: flat on the skull
    S = L_["S"]; T = nrm(np.gradient(S, axis=1))
    for si in range(len(S)):
        P = S[si]; out = np.array([near(p)[1] for p in P]); Bv = nrm(np.cross(T[si], out))
        w = L_["width"] * (1 - 0.30 * uN) * rng.uniform(0.85, 1.15)
        card(P, Bv, w, rng.choice(L_["cols"]), HEAD, twist_sd=0.0, outv=out)
for P, Nn, w0 in BUN_S:                                             # bun coils: flat on the bun surface
    T = nrm(np.gradient(P, axis=0)); Bv = nrm(np.cross(T, Nn))
    card(P, Bv, w0 * (1 - 0.25 * np.linspace(0, 1, len(P))), rng.choice((0, 1, 2, 3, 4, 5)), HEAD, twist_sd=5.0, outv=Nn)
for ci, lo in enumerate(LOOSE):                                     # loose locks: several twisted cards around a guide
    G = lo["G"]; T = nrm(np.gradient(G, axis=0)); uG = np.linspace(0, 1, len(G))
    out = np.array([near(p)[1] for p in G])
    for _ in range(lo["cards"]):
        a = rng.uniform(0, 2 * np.pi); r = rng.uniform(0.0, 0.0035)
        Bv0 = nrm(np.cross(T, out)); Nv = nrm(np.cross(Bv0, T))
        P = G + (Bv0 * math.cos(a) + Nv * math.sin(a)) * r * (0.4 + 0.6 * uG)[:, None]
        w = lo["width"] * (1 - 0.55 * uG) * rng.uniform(0.8, 1.2)
        card(P, Bv0, w, rng.choice(lo["cols"]), lambda u, ci=ci: chain_weights(ci, u), twist_sd=25.0, outv=Nv)
n_cards = len(faces)
# cap: the scalp faces lifted 1.4 mm; U wraps around the bun axis, V runs toward the bun (strand direction)
cap_faces = [tuple(p.vertices) for p in body.data.polygons if all(sv[i] and V[i][2] > hairline_z(V[i]) + 0.006 for i in p.vertices)]
cap_ids = sorted({i for f in cap_faces for i in f}); remap = {}
for i in cap_ids:
    loc, n, _ = near(V[i]); remap[i] = len(verts); verts.append(V[i] + n * 0.0014)
    d = V[i] - BUN_C; ang = math.atan2(d @ E2, d @ E1)
    uvs.append((3.0 * (ang / (2 * math.pi) + 0.5), np.linalg.norm(d) / 0.05)); vw.append({"head": 1.0})
import bmesh
bm = bmesh.new(); bvs = {i: bm.verts.new(tuple(V[i])) for i in cap_ids}
for f in cap_faces:
    try: bm.faces.new([bvs[i] for i in f])
    except ValueError: pass
bmesh.ops.subdivide_edges(bm, edges=bm.edges[:], cuts=2, use_grid_fill=True)
bm.verts.ensure_lookup_table(); bm.faces.ensure_lookup_table()
del verts[len(verts) - len(cap_ids):]; del uvs[len(uvs) - len(cap_ids):]; del vw[len(vw) - len(cap_ids):]
cbase = len(verts)
for bv in bm.verts:
    pv = np.array(bv.co); loc, n, _ = near(pv); pv = loc + n * 0.0016; verts.append(pv)
    d = pv - BUN_C; ang = math.atan2(d @ E2, d @ E1)
    uvs.append((3.0 * (ang / (2 * math.pi) + 0.5), np.linalg.norm(d) / 0.05)); vw.append({"head": 1.0})
for bf in bm.faces: faces.append(tuple(cbase + v.index for v in bf.verts)); fmat.append(1)
n_cap = len(bm.faces); bm.free()
mesh = bpy.data.meshes.new("vivian_hair"); mesh.from_pydata([tuple(x) for x in verts], [], faces); mesh.update()
uvl = mesh.uv_layers.new(name="UVMap")
loop_uv = np.array([uvs[li.vertex_index] for li in mesh.loops], np.float32); uvl.data.foreach_set("uv", loop_uv.ravel())
for p, mi in zip(mesh.polygons, fmat): p.use_smooth = True; p.material_index = mi
hair = bpy.data.objects.new("vivian_hair", mesh); bpy.context.scene.collection.objects.link(hair)
groups = {}
for i, wk in enumerate(vw):
    for g, w in wk.items():
        if w < 1e-3: continue
        vg = groups.get(g) or hair.vertex_groups.new(name=g); groups[g] = vg
        vg.add([i], float(w), "REPLACE")
hair.parent = rig; am = hair.modifiers.new("Armature", "ARMATURE"); am.object = rig
am.use_deform_preserve_volume = True   # dual quaternion, as the engine skins (mesh.vert)
log(f"hair mesh: {n_cards} card quads + {n_cap} cap faces, {len(verts)} verts, {len(LOOSE)} loose locks (physics)")

def hex_lin(h):
    c = np.array([int(h[i:i + 2], 16) for i in (1, 3, 5)]) / 255.0
    return tuple(np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4))
def hair_mat(name, tex, alpha):
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; nt.nodes.clear()
    o_ = nt.nodes.new("ShaderNodeOutputMaterial"); bs = nt.nodes.new("ShaderNodeBsdfPrincipled"); nt.links.new(bs.outputs[0], o_.inputs[0])
    ti = nt.nodes.new("ShaderNodeTexImage"); ti.image = bpy.data.images.load(tex)
    mx = nt.nodes.new("ShaderNodeMix"); mx.data_type = "RGBA"; mx.blend_type = "MULTIPLY"; mx.inputs["Factor"].default_value = 1
    mx.inputs[7].default_value = (*hex_lin(HAIR_HEX), 1); mx.name = "HAIR_TINT"
    nt.links.new(ti.outputs[0], mx.inputs[6]); nt.links.new(mx.outputs[2], bs.inputs["Base Color"])
    if alpha: nt.links.new(ti.outputs[1], bs.inputs["Alpha"]); m.blend_method = "HASHED"
    bs.inputs["Roughness"].default_value = 0.6; bs.inputs["Specular IOR Level"].default_value = 0.25
    m.use_backface_culling = False
    return m
hair.data.materials.append(hair_mat("vivian_hair", TEX + "/vivian_hair.png", True))
hair.data.materials.append(hair_mat("vivian_hair_cap", TEX + "/vivian_hair_cap.png", False))

# ------------------------------------------------------------------ scalp paint under the hairline, all tones
z = np.load(OUT + ("/bakecache5.npz" if os.path.exists(OUT + "/bakecache5.npz") else "/bakecache4.npz")); POS, COV, MH, HP = z["POS"], z["COV"], z["MH"], z["HP"]
hz = hairline_z(POS)
scalp = COV & (MH > 0.5) & (ear_dist(POS) > 0.02)
feather = ss(-0.004, 0.006, POS[..., 2] - hz) * scalp
strand = 0.55 + 0.45 * ss(0.3, 0.7, HP[..., 1])
hl = np.array(hex_lin(HAIR_HEX))
for tone in ("porcelain", "fair", "honey", "olive", "caramel", "bronze", "deep", "ebony"):
    p = f"{TEX}/vivian_skin_{tone}.jpg"
    if not os.path.exists(p): continue
    a = np.flipud(np.asarray(Image.open(p).convert("RGB"), np.float32) / 255.0)
    lin = np.where(a <= 0.04045, a / 12.92, ((a + 0.055) / 1.055) ** 2.4)
    tgt = hl[None, None] * strand[..., None] * 0.9
    lin = lin + (tgt - lin) * (feather * 0.92)[..., None]
    a = np.where(lin <= 0.0031308, lin * 12.92, 1.055 * np.clip(lin, 0, 1) ** (1 / 2.4) - 0.055)
    Image.fromarray(np.flipud(np.clip(a * 255 + 0.5, 0, 255).astype(np.uint8))).save(p, quality=92)
for img_ in bpy.data.images:
    if img_.filepath and "vivian_skin_" in img_.filepath: img_.reload()
log(f"scalp painted under the hairline ({time.time()-t0:.0f}s)")
bpy.ops.wm.save_as_mainfile(filepath=OUT + "/vivian_s5.blend")
log("stage5 (updo hair) saved")
