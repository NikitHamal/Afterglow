# Marcus stage B (v2): natural genitals, part of the body mesh (seamless base).
# REST shape = flaccid: a soft circumcised penis (~9.8 cm build scale) hanging forward-down along the bind axis with a
# gentle drape toward the scrotum; the "aroused" shape key turns it into the erect shaft (~16.5 cm, slight dorsal curve,
# defined corona/frenulum/urethral ridge) along the SAME axis (the game raises it with the 竿 bones), widens the base and
# tightens + lifts the scrotum. penis_length / penis_girth are erect-state deltas. The scrotum is a two-lobed, slightly
# asymmetric sack (left side lower) with its own "scrotum" weight group for a gravity bone.
# UV contract with paint.py: shaft u 0..0.62 around (u=0 ventral seam), v = erect-normalised station (corona at
# 0.8186, tip 1.0); sack u 0.64..1 around, v along.
#   blender -b out/marcus_a.blend --python male_genitals.py
import bpy, bmesh, os, math, json, time, shlex
import numpy as np
from mathutils import Vector
M = "/root/agb/male"; OUT = M + "/out"
def prog(*a): os.system("python3 /root/agb/progress.py " + " ".join(shlex.quote(str(x)) for x in a))
def log(*a): print("[male]", *a, flush=True); prog("log", "[M] " + " ".join(str(x) for x in a))
body = bpy.data.objects["Human"]; rig = bpy.data.objects["Human.rig"]
P_PELVIS = np.array(rig.matrix_world @ rig.data.bones["pelvis"].head_local)
bm = bmesh.new(); bm.from_mesh(body.data); bm.verts.ensure_lookup_table()
SH = bm.verts.layers.shape; BASIS = SH["Basis"]; LAYERS = list(SH.keys())
dl = bm.verts.layers.deform.active
V = np.array([v.co for v in bm.verts])

# ------------------------------------------------------------------ landmarks
mid = np.where(np.abs(V[:, 0]) < 0.004)[0]
low = mid[(V[mid, 2] < P_PELVIS[2]) & (V[mid, 2] > P_PELVIS[2] - 0.25)]
cr = low[np.argmin(V[low, 2])]; CR = V[cr]
front = low[V[low, 1] < CR[1] - 0.02]
zs = np.linspace(CR[2] + 0.01, CR[2] + 0.13, 25)
prof = [(z, V[front[np.abs(V[front, 2] - z) < 0.006], 1].min()) for z in zs if (np.abs(V[front, 2] - z) < 0.006).any()]
PZ, PY = [p[0] for p in prof], [p[1] for p in prof]
BZ = CR[2] + 0.062
B = np.array([0.0, float(np.interp(BZ, PZ, PY)), BZ])
EL = -1.05                                                              # bind axis: 60 deg below horizontal, forward
A = np.array([0.0, -math.cos(EL), math.sin(EL)]); X = np.array([1.0, 0.0, 0.0]); D = np.cross(A, X)   # axis, lateral, dorsal
log("penis base", B.round(3).tolist(), "bind axis", A.round(3).tolist())

# ------------------------------------------------------------------ shaft profiles (shared ring roles, two states)
SC_OLD, L_OLD = 0.176, 0.215
ST_SHAFT = np.array([0.006, 0.016, 0.032] + list(np.linspace(0.055, SC_OLD - 0.012, 6)))
ST_GLANS = SC_OLD + np.array([-0.006, -0.002, 0.002, 0.007, 0.014, 0.022, 0.029, 0.034, 0.0375])
UV_V = np.concatenate([ST_SHAFT, ST_GLANS]) / L_OLD                    # corona at (0.176)/0.215 = 0.8186
STATES = {
    #           length  glans   base r  shaft radii at (base, 1/3, 2/3, corona-)       glans radii (9 rings)                                  curve (+dorsal / -ventral) ellipse lat/dv
    "flaccid": dict(L=0.098, Lg=0.027, r=[0.0174, 0.0150, 0.0142, 0.0139], gl=[0.0143, 0.0140, 0.0148, 0.0149, 0.0146, 0.0136, 0.0118, 0.0090, 0.0050], bend=-0.026, ell=(1.08, 0.92), collar=1.06),
    "erect":   dict(L=0.165, Lg=0.034, r=[0.0218, 0.0200, 0.0192, 0.0181], gl=[0.0180, 0.0172, 0.0207, 0.0206, 0.0198, 0.0180, 0.0152, 0.0110, 0.0056], bend=0.008, ell=(1.03, 0.97), collar=1.0),
}
def state_rings(st, theta, len_k=1.0, girth=1.0):
    S_ = STATES[st]; L = S_["L"] * len_k; Lg = S_["Lg"] * len_k; sc = L - Lg
    s_sh = ST_SHAFT * (sc / SC_OLD); s_gl = sc + (ST_GLANS - SC_OLD) * (Lg / 0.039)
    S = np.concatenate([s_sh, s_gl])
    r_sh = np.interp(s_sh, [s_sh[0], sc / 3, 2 * sc / 3, s_sh[-1]], S_["r"])
    r_sh[-2:] *= [1.0 + (S_["collar"] - 1) * 0.5, S_["collar"]]                # loose skin bunched behind the glans when soft
    R = np.concatenate([r_sh, np.array(S_["gl"])]) * girth
    kap = S_["bend"] / L ** 2
    pts = []
    for k, (s, r) in enumerate(zip(S, R)):
        c = B + A * s + D * kap * s * s
        t = A + D * 2 * kap * s; t /= np.linalg.norm(t); dd = np.cross(t, X); dd /= np.linalg.norm(dd)
        glans = k >= len(s_sh)
        ring = []
        for th in theta:
            rr = r
            dv_ = (th + math.pi / 2 + math.pi) % (2 * math.pi) - math.pi                        # wrap-safe distance from the ventral seam
            if not glans:
                rr += (0.0012 if st == "erect" else 0.0005) * math.exp(-(dv_ / 0.42) ** 2)      # urethral ridge (ventral)
            else:
                notch = 0.07 if st == "erect" else 0.04
                rr *= 1.0 + 0.05 * math.sin(th) - notch * math.exp(-(dv_ / 0.35) ** 2) * (k < len(s_sh) + 3)
            ring.append(c + rr * (S_["ell"][0] * math.cos(th) * X + S_["ell"][1] * math.sin(th) * dd))
        pts.append(ring)
    tip = B + A * L + D * kap * L * L - dd * 0.002
    return np.array(pts), tip

# ------------------------------------------------------------------ local subdivision of the crotch (all layers interpolate)
C_REG = B + np.array([0.0, 0.035, -0.035])
def fcen(f): return np.mean([np.array(v.co) for v in f.verts], 0)
reg = [f for f in bm.faces if np.linalg.norm(fcen(f) - C_REG) < 0.085 and abs(fcen(f)[0]) < 0.075]
edges = list({e for f in reg for e in f.edges})
bmesh.ops.subdivide_edges(bm, edges=edges, cuts=2, use_grid_fill=True, smooth=0.0)
bm.verts.ensure_lookup_table(); bm.faces.ensure_lookup_table()
log(f"crotch subdivided: {len(reg)} faces -> verts now {len(bm.verts)}")

# ------------------------------------------------------------------ round hole at the base (flaccid radius)
R0 = 0.0180
hole = [f for f in bm.faces if np.linalg.norm(fcen(f) - B) < R0 * 0.95]
bmesh.ops.delete(bm, geom=hole, context="FACES")
bnd_e = [e for e in bm.edges if e.is_boundary and np.linalg.norm(np.array(e.verts[0].co) - B) < 0.05]
ring0 = list({v for e in bnd_e for v in e.verts})
def ang(p):
    w = np.array(p) - B; th = math.atan2(w @ D, w @ X)
    return (th + math.pi / 2) % (2 * math.pi)                         # 0 = ventral seam (paint.py convention)
ring0.sort(key=lambda v: ang(v.co))
N = len(ring0)
phi = np.array([ang(v.co) for v in ring0]); theta = phi - math.pi / 2  # back to the X/D angle used by the profiles
for v, th in zip(ring0, theta):
    tgt = B + R0 * 1.02 * (math.cos(th) * X + math.sin(th) * D)
    tgt = tgt - A * ((tgt - B) @ A) + A * ((np.array(v.co) - B) @ A)
    dv = Vector(tgt) - v.co
    v.co += dv
    for L in LAYERS: v[SH[L]] = v[SH[L]] + dv
log(f"hole cut: {len(hole)} faces, boundary ring {N} verts")

# ------------------------------------------------------------------ shaft (flaccid rest geometry)
PF, TIPF = state_rings("flaccid", theta)
PE, TIPE = state_rings("erect", theta)
PEL, TIPEL = state_rings("erect", theta, len_k=1.18)
PEG, TIPEG = state_rings("erect", theta, girth=1.12)
rings = [ring0] + [[bm.verts.new(Vector(p)) for p in PF[k]] for k in range(len(PF))]
tipv = bm.verts.new(Vector(TIPF))
newf = []
for k in range(1, len(rings)):
    a_, b_ = rings[k - 1], rings[k]
    for i in range(N):
        j = (i + 1) % N; newf.append(bm.faces.new((a_[i], a_[j], b_[j], b_[i])))
for i in range(N): newf.append(bm.faces.new((rings[-1][i], rings[-1][(i + 1) % N], tipv)))
bmesh.ops.recalc_face_normals(bm, faces=newf)
for f in newf: f.smooth = True; f.material_index = 1
def layer_off(ring, L): return np.mean([np.array(v[SH[L]]) - np.array(v[BASIS]) for v in ring], 0)
for L in LAYERS:
    o = layer_off(ring0, L)
    for k in range(len(PF)):
        for i, v in enumerate(rings[k + 1]): v[SH[L]] = Vector(PF[k][i] + o)
    tipv[SH[L]] = Vector(TIPF + o)
log(f"shaft built: {len(PF)} rings x {N} + tip (flaccid rest)")

# ------------------------------------------------------------------ scrotum
bm.normal_update(); bm.faces.ensure_lookup_table()
HZ = B[2] - 0.044
HC = np.array([0.0, float(np.interp(HZ, PZ, PY)), HZ])
AXS = {"rest": np.array([0.0, -0.20, -0.98]), "tight": np.array([0.0, -0.34, -0.94])}
for k_ in AXS: AXS[k_] /= np.linalg.norm(AXS[k_])
def sframe(ax):
    sx = np.array([1.0, 0.0, 0.0]); sd = np.cross(ax, sx); sd /= np.linalg.norm(sd); return sx, sd
SX0, SD0 = sframe(AXS["rest"])
def ell(p):
    w = p - HC; return (w[0] / 0.031) ** 2 + (np.linalg.norm(w[1:]) / 0.016) ** 2
shaft_faces = set(newf)
sack_hole = [f for f in bm.faces if f not in shaft_faces and np.linalg.norm(fcen(f) - HC) < 0.04 and ell(fcen(f)) < 1.0]
bmesh.ops.delete(bm, geom=sack_hole, context="FACES")
sb = [e for e in bm.edges if e.is_boundary and np.linalg.norm(np.array(e.verts[0].co) - HC) < 0.045]
sring0 = list({v for e in sb for v in e.verts})
def sang(p):
    w = np.array(p) - HC; return math.atan2(w @ SD0, w @ SX0)
sring0.sort(key=lambda v: sang(v.co)); NS = len(sring0); sth = np.array([sang(v.co) for v in sring0])
SS = np.array([0.006, 0.016, 0.028, 0.041, 0.054, 0.065, 0.073, 0.078])
SR = np.array([0.024, 0.029, 0.0318, 0.0330, 0.0322, 0.0284, 0.0212, 0.0110])
def sack_pts(state):
    ax = AXS["rest" if state == "rest" else "tight"]; sx, sd = sframe(ax)
    lk, rk = (1.0, 1.0) if state == "rest" else (0.80, 0.93)
    base = np.array([np.array(v[BASIS]) for v in sring0])
    out = []
    for k, (s_, r_) in enumerate(zip(SS * lk, SR * rk)):
        blend = min(1.0, (k + 1) / 2.5)
        ring = []
        for i, th in enumerate(sth):
            side = math.cos(th)                                        # +1 = his left (+X)
            drop = (0.007 if state == "rest" else 0.004) * max(0.0, side) * min(1.0, k / 4.0)   # left testicle hangs lower
            c = HC + ax * (s_ + drop)
            lat = 1.28 - 0.06 * (s_ / 0.07)
            groove = 1 - (0.06 + 0.12 * min(1.0, s_ / 0.035)) * math.exp(-((abs(th) - math.pi / 2) / 0.30) ** 2)   # raphe
            p = c + r_ * groove * (lat * math.cos(th) * sx + math.sin(th) * sd)
            p = base[i] + (p - base[i]) * blend if k < 2 else p
            ring.append(p)
        out.append(ring)
    pole = HC + ax * (SS[-1] * lk + 0.004)
    return np.array(out), pole
SP, SPOLE = sack_pts("rest"); SPT, SPOLET = sack_pts("tight")
srings = [sring0] + [[bm.verts.new(Vector(p)) for p in r] for r in SP]
spole = bm.verts.new(Vector(SPOLE))
sackf = []
for k in range(1, len(srings)):
    a_, b_ = srings[k - 1], srings[k]
    for i in range(NS):
        j = (i + 1) % NS; sackf.append(bm.faces.new((a_[i], a_[j], b_[j], b_[i])))
for i in range(NS): sackf.append(bm.faces.new((srings[-1][i], srings[-1][(i + 1) % NS], spole)))
bmesh.ops.recalc_face_normals(bm, faces=sackf)
for f in sackf: f.smooth = True; f.material_index = 1
for L in LAYERS:
    o = layer_off(sring0, L)
    for k in range(len(SP)):
        for i, v in enumerate(srings[k + 1]): v[SH[L]] = Vector(SP[k][i] + o)
    spole[SH[L]] = Vector(SPOLE + o)
sack_set = set(v for r in srings[1:] for v in r) | {spole}
log(f"scrotum built: {len(SP)} rings x {NS}, left side lower")

# clearance check: soft shaft must rest on (not inside) the sack
def mind(Pa, Pb): return float(np.min(np.linalg.norm(Pa[:, None, :] - Pb[None, :, :], axis=-1)))
gap = mind(PF.reshape(-1, 3), SP.reshape(-1, 3))
log(f"flaccid shaft <-> scrotum surface clearance {gap*1000:.1f} mm")

# ------------------------------------------------------------------ smooth the junction region (delta applied to all layers)
shaft_set = set(v for r in rings[1:] for v in r) | {tipv}
region_v = [v for v in bm.verts if np.linalg.norm(np.array(v.co) - (B + np.array([0, 0.02, -0.03]))) < 0.075 and v not in shaft_set and v not in sack_set]
for it in range(2):
    deltas = {}
    for v in region_v:
        nb = [e.other_vert(v) for e in v.link_edges]
        if not nb or v.is_boundary: continue
        avg = sum((u.co for u in nb), Vector()) / len(nb)
        deltas[v] = (avg - v.co) * (0.25 if v not in ring0 else 0.12)
    for v, dv in deltas.items():
        v.co += dv
        for L in LAYERS: v[SH[L]] = v[SH[L]] + dv

# ------------------------------------------------------------------ UVs + weights
uvl = bm.loops.layers.uv.active
gi = {vg.name: vg.index for vg in body.vertex_groups}
for nm in ("shaft_root", "shaft_mid", "shaft_tip", "scrotum"):
    if nm not in gi: gi[nm] = body.vertex_groups.new(name=nm).index
ridx = {v: (k, i) for k, r in enumerate(rings) for i, v in enumerate(r)}
VFULL = np.concatenate([[0.0], UV_V])
for f in newf:
    ks = [ridx[l.vert] for l in f.loops if l.vert in ridx]
    wrap = max(i for _, i in ks) == N - 1 and any(i == 0 for _, i in ks)
    for l in f.loops:
        if l.vert is tipv:
            l[uvl].uv = (0.62 * np.mean([(i + (N if wrap and i == 0 else 0)) for _, i in ks]) / N, 1.0); continue
        k, i = ridx[l.vert]
        l[uvl].uv = (0.62 * (i + (N if wrap and i == 0 else 0)) / N, VFULL[k] * 0.985)
sidx = {v: (k, i) for k, r in enumerate(srings) for i, v in enumerate(r)}
SSn = [0.0] + list(SS / (SS[-1] + 0.004))
for f in sackf:
    ks = [sidx[l.vert] for l in f.loops if l.vert in sidx]
    wrap = max(i for _, i in ks) == NS - 1 and any(i == 0 for _, i in ks)
    for l in f.loops:
        if l.vert is spole:
            l[uvl].uv = (0.64 + 0.36 * np.mean([(i + (NS if wrap and i == 0 else 0)) for _, i in ks]) / NS, 0.99); continue
        k, i = sidx[l.vert]
        l[uvl].uv = (0.64 + 0.36 * (i + (NS if wrap and i == 0 else 0)) / NS, 0.02 + 0.95 * SSn[k])
for k, r in enumerate(srings[1:], start=1):                            # sack: pelvis near the body, gravity bone below
    w_s = min(1.0, max(0.0, (k - 1) / 4.0)) * 0.9
    for v in r:
        dv = v[dl]; dv.clear(); dv[gi["pelvis"]] = 1.0 - w_s
        if w_s > 1e-3: dv[gi["scrotum"]] = w_s
spole[dl].clear(); spole[dl][gi["pelvis"]] = 0.1; spole[dl][gi["scrotum"]] = 0.9
for k, r in enumerate(rings):
    if k == 0: continue
    sn = VFULL[k]
    w_root = max(0.0, 1 - sn / 0.45); w_tip = max(0.0, (sn - 0.55) / 0.45); w_mid = max(0.0, 1 - w_root - w_tip)
    pel = 0.5 if k == 1 else (0.2 if k == 2 else 0.0)
    for v in r:
        dv = v[dl]; dv.clear()
        if pel: dv[gi["pelvis"]] = pel
        for nm, w in (("shaft_root", w_root * (1 - pel)), ("shaft_mid", w_mid * (1 - pel)), ("shaft_tip", w_tip * (1 - pel))):
            if w > 1e-3: dv[gi[nm]] = w
tipv[dl].clear(); tipv[dl][gi["shaft_tip"]] = 1.0
bm.verts.index_update(); bm.verts.ensure_lookup_table()
shaft_idx = [v.index for r in rings[1:] for v in r] + [tipv.index]
ring0_idx = [v.index for v in ring0]
sack_idx = [v.index for r in srings[1:] for v in r] + [spole.index]
bm.normal_update()
near_idx = [v.index for v in bm.verts if v not in shaft_set and v not in sack_set and v not in ring0
            and np.linalg.norm(np.array(v.co) - B) < 0.03]
near_nrm = np.array([np.array(bm.verts[i].normal) for i in near_idx]) if near_idx else np.zeros((0, 3))
near_w = np.array([max(0.0, 1 - np.linalg.norm(np.array(bm.verts[i].co) - B) / 0.03) for i in near_idx])
bm.to_mesh(body.data); bm.free(); body.data.update()

# ------------------------------------------------------------------ aroused / length / girth keys
basis = np.array([v.co for v in body.data.shape_keys.key_blocks["Basis"].data])
def put(a, P, T): a[shaft_idx] = np.concatenate([P.reshape(-1, 3), T[None]], 0)
ar = basis.copy(); put(ar, PE, TIPE)
for i in ring0_idx:                                                    # base widens to the erect girth
    w = ar[i] - B; along = A * (w @ A); rad = w - along
    ar[i] = B + along + rad / max(1e-6, np.linalg.norm(rad)) * 0.0222
if near_idx: ar[near_idx] += near_nrm * (0.0022 * near_w)[:, None]   # engorged base swells the pubic skin a little
ar[sack_idx] = np.concatenate([SPT.reshape(-1, 3), SPOLET[None]], 0)  # scrotum tightens and lifts
kb = body.shape_key_add(name="aroused", from_mix=False); kb.data.foreach_set("co", ar.ravel())
for nm, (Pk, Tk) in (("penis_length", (PEL, TIPEL)), ("penis_girth", (PEG, TIPEG))):
    a = basis.copy()
    d = np.concatenate([Pk.reshape(-1, 3), Tk[None]], 0) - np.concatenate([PE.reshape(-1, 3), TIPE[None]], 0)
    a[shaft_idx] += d                                                  # erect-state delta (game scales it by arousal)
    kb = body.shape_key_add(name=nm, from_mix=False); kb.data.foreach_set("co", a.ravel())
if len(body.material_slots) < 2:
    gm = bpy.data.materials.new("marcus_genital"); body.data.materials.append(gm)
json.dump({"B": B.tolist(), "A": A.tolist(), "D": D.tolist(), "tip": TIPF.tolist(), "tip_erect": TIPE.tolist(),
           "L_flaccid": STATES["flaccid"]["L"], "L_erect": STATES["erect"]["L"], "N": N, "shaft_idx": shaft_idx,
           "crotch": CR.tolist(), "sack_center": HC.tolist(), "sack_axis": AXS["rest"].tolist()},
          open(OUT + "/genital.json", "w"))
bpy.ops.wm.save_as_mainfile(filepath=OUT + "/marcus_b.blend")
log(f"stage B saved: body {len(body.data.vertices)} verts, keys {[k.name for k in body.data.shape_keys.key_blocks]}")
