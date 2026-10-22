# Marcus stage B: anatomically built genitals, part of the body mesh (seamless base).
# Crotch locally subdivided; a round hole at the pubic base; an erect, slightly up-curved shaft with corona,
# sulcus, frenulum notch, urethral ridge and glans grown from that boundary loop; scrotum sculpted by displacement.
# Every shape layer (expressions, body variants) is carried through; penis_length / penis_girth keys added.
#   blender -b out/marcus_a.blend --python male_genitals.py
import bpy, bmesh, os, math, json, time, shlex
import numpy as np
from mathutils import Vector
M = "/root/agb/male"; OUT = M + "/out"
def prog(*a): os.system("python3 /root/agb/progress.py " + " ".join(shlex.quote(str(x)) for x in a))
def log(*a): print("[male]", *a, flush=True); prog("log", "[male] " + " ".join(str(x) for x in a))
body = bpy.data.objects["Human"]; rig = bpy.data.objects["Human.rig"]
P_PELVIS = np.array(rig.matrix_world @ rig.data.bones["pelvis"].head_local)
bm = bmesh.new(); bm.from_mesh(body.data); bm.verts.ensure_lookup_table()
SH = bm.verts.layers.shape; BASIS = SH["Basis"]; LAYERS = list(SH.keys())
dl = bm.verts.layers.deform.active
V = np.array([v.co for v in bm.verts])

# ------------------------------------------------------------------ landmarks
mid = np.where(np.abs(V[:, 0]) < 0.004)[0]
low = mid[(V[mid, 2] < P_PELVIS[2]) & (V[mid, 2] > P_PELVIS[2] - 0.25)]
cr = low[np.argmin(V[low, 2])]; CR = V[cr]                               # crotch (lowest midline point between the legs)
front = low[V[low, 1] < CR[1] - 0.02]
zs = np.linspace(CR[2] + 0.01, CR[2] + 0.13, 25)
prof = [(z, V[front[np.abs(V[front, 2] - z) < 0.006], 1].min()) for z in zs if (np.abs(V[front, 2] - z) < 0.006).any()]
log("crotch", CR.round(3).tolist(), "front profile", [(round(z, 3), round(y, 3)) for z, y in prof[::3]])
BZ = CR[2] + 0.062
BY = float(np.interp(BZ, [p[0] for p in prof], [p[1] for p in prof]))
B = np.array([0.0, BY, BZ])
EL = 0.17                                                              # rest elevation of the shaft (rad, above horizontal)
A = np.array([0.0, -math.cos(EL), math.sin(EL)]); X = np.array([1.0, 0.0, 0.0]); D = np.cross(A, X)   # axis, lateral, dorsal
log("penis base", B.round(3).tolist(), "axis", A.round(3).tolist())

# ------------------------------------------------------------------ local subdivision of the crotch (index based, all layers interpolate)
C_REG = B + np.array([0.0, 0.035, -0.035])
def fcen(f): return np.mean([np.array(v.co) for v in f.verts], 0)
reg = [f for f in bm.faces if np.linalg.norm(fcen(f) - C_REG) < 0.085 and abs(fcen(f)[0]) < 0.075]
edges = list({e for f in reg for e in f.edges})
bmesh.ops.subdivide_edges(bm, edges=edges, cuts=2, use_grid_fill=True, smooth=0.0)
bm.verts.ensure_lookup_table(); bm.faces.ensure_lookup_table()
log(f"crotch subdivided: {len(reg)} faces -> verts now {len(bm.verts)}")

# ------------------------------------------------------------------ round hole at the base
R0 = 0.0215
hole = [f for f in bm.faces if np.linalg.norm(fcen(f) - B) < R0 * 0.92]
bmesh.ops.delete(bm, geom=hole, context="FACES")
bnd_e = [e for e in bm.edges if e.is_boundary and np.linalg.norm(np.array(e.verts[0].co) - B) < 0.05]
ring0 = list({v for e in bnd_e for v in e.verts})
def ang(p):
    w = np.array(p) - B; return math.atan2(w @ D, w @ X)
ring0.sort(key=lambda v: ang(v.co))
N = len(ring0)
log(f"hole cut: {len(hole)} faces, boundary ring {N} verts")
theta = np.array([ang(v.co) for v in ring0])
# make the junction round (all layers move by the same correction)
for v, th in zip(ring0, theta):
    tgt = B + R0 * 1.02 * (math.cos(th) * X + math.sin(th) * D)
    tgt = tgt - A * ((tgt - B) @ A) + A * ((np.array(v.co) - B) @ A)          # keep its depth along the axis
    dv = Vector(tgt) - v.co
    v.co += dv
    for L in LAYERS: v[SH[L]] = v[SH[L]] + dv

# ------------------------------------------------------------------ shaft rings
def profile(Lt=0.215, girth=1.0):
    """fixed ring count (topology identical for the length/girth keys)"""
    Lg = 0.039; sc = Lt - Lg                                           # corona station
    shaft_s = [0.006, 0.016, 0.032] + list(np.linspace(0.055, sc - 0.012, 6))
    shaft_r = np.interp(shaft_s, [0.006, 0.016, 0.032, 0.055, sc - 0.012], [0.0228, 0.0212, 0.0203, 0.0200, 0.0181])
    gl_s = [sc - 0.006, sc - 0.002, sc + 0.002, sc + 0.007, sc + 0.014, sc + 0.022, sc + 0.029, sc + 0.034, sc + 0.0375]
    gl_r = [0.0179, 0.0171, 0.0224, 0.0223, 0.0214, 0.0194, 0.0163, 0.0118, 0.0060]
    return np.array(shaft_s + gl_s), np.concatenate([shaft_r, gl_r]) * girth, Lt, sc
def ring_pts(Lt=0.215, girth=1.0, off=np.zeros(3)):
    S, R, Lt_, sc = profile(Lt, girth)
    kap = 0.013 / Lt_ ** 2
    pts = []
    for s, r in zip(S, R):
        c = B + A * s + D * kap * s * s
        t = A + D * 2 * kap * s; t /= np.linalg.norm(t); dd = np.cross(t, X); dd /= np.linalg.norm(dd)
        glans = s > sc - 0.004
        ring = []
        for th in theta:
            rr = r
            if not glans: rr += 0.0011 * math.exp(-((th + math.pi / 2) / 0.42) ** 2)          # urethral ridge (ventral)
            else:
                rr *= 1.0 + 0.05 * math.sin(th) - 0.07 * math.exp(-((th + math.pi / 2) / 0.35) ** 2) * (s < sc + 0.01)   # dorsal corona, frenulum notch
            ring.append(c + rr * (1.03 * math.cos(th) * X + 0.97 * math.sin(th) * dd) + off)
        pts.append(ring)
    tip = B + A * Lt_ + D * kap * Lt_ * Lt_ - dd * 0.0022 + off
    return np.array(pts), tip, S / Lt_
P, TIP, SN = ring_pts()
rings = [ring0]
for k in range(len(P)):
    rings.append([bm.verts.new(Vector(p)) for p in P[k]])
tipv = bm.verts.new(Vector(TIP))
newf = []
for k in range(1, len(rings)):
    a_, b_ = rings[k - 1], rings[k]
    for i in range(N):
        j = (i + 1) % N
        newf.append(bm.faces.new((a_[i], a_[j], b_[j], b_[i])))
last = rings[-1]
for i in range(N):
    newf.append(bm.faces.new((last[i], last[(i + 1) % N], tipv)))
bmesh.ops.recalc_face_normals(bm, faces=newf)
for f in newf: f.smooth = True
# shape layers for the new verts: body variants shift the base, everything else rides along
r0 = np.array([np.array(v[BASIS]) for v in ring0])
def layer_off(L): return np.mean([np.array(v[SH[L]]) - np.array(v[BASIS]) for v in ring0], 0)
PL, TL, _ = ring_pts(Lt=0.215 * 1.32); PG, TG, _ = ring_pts(girth=1.17)
for L in LAYERS:
    o = layer_off(L)
    for k in range(len(P)):
        for i, v in enumerate(rings[k + 1]): v[SH[L]] = Vector(P[k][i] + o)
    tipv[SH[L]] = Vector(TIP + o)
log(f"shaft built: {len(P)} rings x {N} + tip")

# ------------------------------------------------------------------ scrotum: a sack grown from an elliptical hole below the shaft base
bm.normal_update(); bm.faces.ensure_lookup_table()
HZ = B[2] - 0.041
HC = np.array([0.0, float(np.interp(HZ, [p[0] for p in prof], [p[1] for p in prof])), HZ])   # on the crotch surface, below the base
AX = np.array([0.0, -0.42, -0.91]); AX /= np.linalg.norm(AX)    # hangs down and a little forward
SX = np.array([1.0, 0.0, 0.0]); SD = np.cross(AX, SX); SD /= np.linalg.norm(SD)
def ell(p):
    w = p - HC; return (w[0] / 0.031) ** 2 + (np.linalg.norm(w[1:]) / 0.016) ** 2
shaft_faces = set(newf)
sack_hole = [f for f in bm.faces if f not in shaft_faces and np.linalg.norm(fcen(f) - HC) < 0.04 and ell(fcen(f)) < 1.0]
bmesh.ops.delete(bm, geom=sack_hole, context="FACES")
sb = [e for e in bm.edges if e.is_boundary and np.linalg.norm(np.array(e.verts[0].co) - HC) < 0.045]
sring0 = list({v for e in sb for v in e.verts})
def sang(p):
    w = np.array(p) - HC; return math.atan2(w @ SD, w @ SX)
sring0.sort(key=lambda v: sang(v.co)); NS = len(sring0); sth = np.array([sang(v.co) for v in sring0])
log(f"sack hole: {len(sack_hole)} faces, ring {NS}")
SS = [0.006, 0.015, 0.026, 0.038, 0.050, 0.060, 0.068, 0.0735]
SR = [0.024, 0.0275, 0.0300, 0.0312, 0.0302, 0.0262, 0.0185, 0.0085]
def sack_pts(off=np.zeros(3)):
    out = []
    base = np.array([np.array(v[BASIS]) for v in sring0])
    for k, (s_, r_) in enumerate(zip(SS, SR)):
        c = HC + AX * s_
        blend = min(1.0, (k + 1) / 2.5)                              # first rings start from the hole outline
        ring = []
        for i, th in enumerate(sth):
            lat = 1.30 - 0.06 * (s_ / 0.07)
            groove = 1 - (0.06 + 0.12 * min(1.0, s_ / 0.035)) * math.exp(-((abs(th) - math.pi / 2) / 0.30) ** 2)   # raphe: two testicles
            p = c + r_ * groove * (lat * math.cos(th) * SX + math.sin(th) * SD)
            p = base[i] + (p - base[i]) * blend if k < 2 else p
            ring.append(p + off)
        out.append(ring)
    pole = HC + AX * (SS[-1] + 0.004) + off
    return np.array(out), pole
SP, SPOLE = sack_pts()
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
def soff(L): return np.mean([np.array(v[SH[L]]) - np.array(v[BASIS]) for v in sring0], 0)
for L in LAYERS:
    o = soff(L)
    for k in range(len(SP)):
        for i, v in enumerate(srings[k + 1]): v[SH[L]] = Vector(SP[k][i] + o)
    spole[SH[L]] = Vector(SPOLE + o)
sack_set = set(v for r in srings[1:] for v in r) | {spole}
log(f"scrotum sack built: {len(SP)} rings x {NS}")

# ------------------------------------------------------------------ smooth the junction + scrotum region (delta applied to all layers)
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

# ------------------------------------------------------------------ shaft material slot + UVs, weights
uvl = bm.loops.layers.uv.active
gi_pelvis = body.vertex_groups['pelvis'].index
for f in newf: f.material_index = 1
ridx = {}
for k, r in enumerate(rings):
    for i, v in enumerate(r): ridx[v] = (k, i)
SNfull = np.concatenate([[0.0], SN])
for f in newf:
    ks = [ridx[l.vert] for l in f.loops if l.vert in ridx]
    imax = max(i for _, i in ks); wrap = imax == N - 1 and any(i == 0 for _, i in ks)
    for l in f.loops:
        if l.vert is tipv:
            l[uvl].uv = (0.62 * np.mean([(i + (N if wrap and i == 0 else 0)) for _, i in ks]) / N, 1.0); continue
        k, i = ridx[l.vert]
        u = (i + (N if wrap and i == 0 else 0)) / N
        l[uvl].uv = (u * 0.62, SNfull[k] * 0.985)
sidx = {}
for k, r in enumerate(srings):
    for i, v in enumerate(r): sidx[v] = (k, i)
SSn = [0.0] + [x / (SS[-1] + 0.004) for x in SS]
for f in sackf:
    ks = [sidx[l.vert] for l in f.loops if l.vert in sidx]
    wrap = max(i for _, i in ks) == NS - 1 and any(i == 0 for _, i in ks)
    for l in f.loops:
        if l.vert is spole:
            l[uvl].uv = (0.64 + 0.36 * np.mean([(i + (NS if wrap and i == 0 else 0)) for _, i in ks]) / NS, 0.99); continue
        k, i = sidx[l.vert]
        l[uvl].uv = (0.64 + 0.36 * (i + (NS if wrap and i == 0 else 0)) / NS, 0.02 + 0.95 * SSn[k])
for v in sack_set:
    dv = v[dl]; dv.clear(); dv[gi_pelvis] = 1.0
# deform weights: base ring blends pelvis -> shaft root; chain weights along the length
gi = {vg.name: vg.index for vg in body.vertex_groups}
for nm in ("shaft_root", "shaft_mid", "shaft_tip"):
    if nm not in gi: gi[nm] = body.vertex_groups.new(name=nm).index
for k, r in enumerate(rings):
    if k == 0: continue
    sn = SNfull[k]
    w_root = max(0.0, 1 - sn / 0.45); w_tip = max(0.0, (sn - 0.55) / 0.45); w_mid = max(0.0, 1 - w_root - w_tip)
    pel = 0.5 if k == 1 else (0.2 if k == 2 else 0.0)
    for v in r:
        dv = v[dl]; dv.clear()
        if pel: dv[gi["pelvis"]] = pel
        for nm, w in (("shaft_root", w_root * (1 - pel)), ("shaft_mid", w_mid * (1 - pel)), ("shaft_tip", w_tip * (1 - pel))):
            if w > 1e-3: dv[gi[nm]] = w
tipv[dl].clear(); tipv[dl][gi["shaft_tip"]] = 1.0
bm.verts.index_update()
shaft_idx = [v.index for r in rings[1:] for v in r] + [tipv.index]
bm.to_mesh(body.data); bm.free(); body.data.update()
# penis_length / penis_girth keys (shaft verts only)
basis_co = np.array([v.co for v in body.data.shape_keys.key_blocks["Basis"].data])
for nm, Pk, Tk in (("penis_length", PL, TL), ("penis_girth", PG, TG)):
    kb = body.shape_key_add(name=nm, from_mix=False); a = basis_co.copy()
    flat = np.concatenate([Pk.reshape(-1, 3), Tk[None]], 0)
    a[shaft_idx] = flat
    kb.data.foreach_set("co", a.ravel())
if len(body.material_slots) < 2:
    gm = bpy.data.materials.new("marcus_genital"); body.data.materials.append(gm)
json.dump({"B": B.tolist(), "A": A.tolist(), "D": D.tolist(), "tip": TIP.tolist(), "N": N, "shaft_idx": shaft_idx, "crotch": CR.tolist()},
          open(OUT + "/genital.json", "w"))
bpy.ops.wm.save_as_mainfile(filepath=OUT + "/marcus_b.blend")
log(f"stage B saved: body {len(body.data.vertices)} verts, keys {[k.name for k in body.data.shape_keys.key_blocks]}")
