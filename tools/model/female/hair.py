# Vivian hair: procedural hair cards grown on the real scalp of the stage-4 body.
# Long layered blow-out waves past the shoulders, deep side part, face-framing front.
#   blender -b out/vivian_s4.blend --python hair.py
import bpy, os, math, json, time, shlex
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from PIL import Image

OUT = "/root/agb/out"; TEX = OUT + "/tex"
HAIR_HEX = "#3a2117"                      # default tint (game re-tints materials named *hair* from the preset colour)
def prog(*a): os.system("python3 /root/agb/progress.py " + " ".join(shlex.quote(str(x)) for x in a))
def log(*a): print("[hair]", *a, flush=True); prog("log", " ".join(str(x) for x in a))
t0 = time.time(); rng = np.random.default_rng(5)
Lm = json.load(open(OUT + "/landmarks.json")); EL, ER = np.array(Lm["EL"]), np.array(Lm["ER"])
bones = {k: (np.array(v[0]), np.array(v[1])) for k, v in Lm["bones"].items()}
EC = (EL + ER) / 2
body = bpy.data.objects["Human"]; rig = bpy.data.objects["Human.rig"]
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
V = np.empty(len(me.vertices) * 3, np.float32); me.vertices.foreach_get("co", V); V = V.reshape(-1, 3)
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
SHZ = bones["upperarm_l"][0][2]                                    # shoulder joint height
headv = V[(wh > 0.5) & (wb > 0.5)]
CHIN_Z = headv[(np.abs(headv[:, 0]) < 0.015) & (headv[:, 1] < EC[1])][:, 2].min()
EARC = [V[(we > 0.5) & (np.sign(V[:, 0]) == s)].mean(0) for s in (1, -1)]
log(f"head centre {Ch.round(3)}, chin z {CHIN_Z:.3f}, shoulders z {SHZ:.3f}")

def phi_of(p): return np.arctan2(p[..., 0] - Ch[0], -(p[..., 1] - Ch[1]))       # 0 = front, ±pi = back
HL_A = [0.0, 0.35, 0.70, 1.00, 1.25, 1.45, 1.75, 2.05, 2.50, math.pi]
HL_Z = [0.070, 0.064, 0.048, 0.032, 0.022, 0.016, 0.004, -0.045, -0.078, -0.088]
def hairline_z(p): return EC[2] + np.interp(np.abs(phi_of(p)), HL_A, HL_Z)
def on_scalp(p):
    far_ear = np.minimum(np.linalg.norm(p - EARC[0], axis=-1), np.linalg.norm(p - EARC[1], axis=-1)) > 0.024
    return (p[..., 2] > hairline_z(p)) & far_ear

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
pick = rng.choice(len(tris), size=40000, p=area / area.sum())
r1, r2 = np.sqrt(rng.random(40000)), rng.random(40000)
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
ROOTS = poisson(cand, 0.0062)
RN = np.array([bvh.find_nearest(tuple(p))[1] for p in ROOTS])
log(f"scalp: {len(tris)} tris, {area.sum()*1e4:.0f} cm2, {len(ROOTS)} roots")

# ------------------------------------------------------------------ growth
PART_X = -0.026                                   # deep side part on her right; volume sweeps to her left
EX, EY, EZ = np.eye(3)
def face_half(z): return 0.060 + 0.010 * ss(EC[2] - 0.02, CHIN_Z, z)
def grow(root, n, vol, tip_z, seg=0.008, max_len=0.75):
    ph = phi_of(root); top = ss(EC[2] + 0.06, EC[2] + 0.11, root[2]); sgn = 1.0 if root[0] >= PART_X else -1.0
    front = abs(ph) < 1.65
    sidek = (0.35 + 0.9 * top) * ss(2.5, 1.5, abs(ph))
    desire = EX * sgn * sidek + EY * (0.35 if not front else 0.1) - EZ * (1.0 - 0.6 * top * ss(2.5, 1.5, abs(ph)))
    if abs(ph) < 0.9: desire += EY * 0.55 + EX * sgn * 0.35            # hairline sweeps back off the face
    f = desire - n * (desire @ n); f /= max(1e-9, np.linalg.norm(f))
    d = f * 0.75 + n * 0.7; d /= np.linalg.norm(d)
    p = root + n * 0.0015; pts = [p.copy()]; s = 0.0
    lim = 1.8 if root[0] > PART_X else 1.25
    shoulder_bias = (-EY * 0.35) if abs(ph) < lim else (EY * 0.45)
    while s < max_len:
        g = ss(0.0, 0.11, s)
        sh = ss(SHZ + 0.13, SHZ + 0.02, p[2]) * ss(0.055, 0.09, abs(p[0]))
        d = d + (-EZ) * (0.06 + 0.32 * g) + f * 0.10 * (1 - g) + shoulder_bias * (0.06 + 0.30 * sh) * ss(0.08, 0.2, s) - EX * np.sign(p[0]) * 0.22 * sh
        d /= np.linalg.norm(d)
        p = p + d * seg
        off = (0.003 + 0.016 * ss(0.0, 0.08, s)) * vol * (1.0 - 0.55 * ss(0.16, 0.30, s))
        loc, nn, _, dist = bvh.find_nearest(tuple(p))
        if loc is not None:
            loc, nn = np.array(loc), np.array(nn)
            if (p - loc) @ nn < 0 or dist < off:
                p = loc + nn * off
                d = d - nn * min(0.0, d @ nn); d /= np.linalg.norm(d)
        if p[1] < EC[1] + 0.025 and CHIN_Z - 0.03 < p[2] < EC[2] + 0.075:   # keep the face clear
            hw = face_half(p[2])
            if abs(p[0]) < hw: p[0] = math.copysign(hw, p[0] if abs(p[0]) > 1e-4 else sgn)
        pts.append(p.copy()); s += seg
        if p[2] < tip_z: break
    return np.array(pts)
def resample(P, n):
    seg = np.linalg.norm(np.diff(P, axis=0), axis=1); cl = np.concatenate([[0], np.cumsum(seg)])
    t = np.linspace(0, cl[-1], n); return np.stack([np.interp(t, cl, P[:, k]) for k in range(3)], 1), cl[-1]
NSEG = 22
def tip_target(root):
    ph = abs(phi_of(root))
    if ph < 0.9: return SHZ - 0.03 + rng.uniform(-0.035, 0.03)          # face-framing layers end around the collarbone
    return SHZ - 0.10 - 0.08 * ss(1.2, math.pi, ph) + rng.uniform(-0.055, 0.035)   # U hemline, longest at the centre back
layers = []
for li, (vol, frac, width, cols) in enumerate([(0.6, 1.0, 0.0155, (0, 1, 2, 3)), (1.0, 0.72, 0.0100, (4, 5, 6, 7))]):
    idx = np.where(rng.random(len(ROOTS)) < frac)[0]
    S = []
    for i in idx:
        r = ROOTS[i] + (rng.normal(0, 0.0015, 3) if li else 0)
        P, ln = resample(grow(r, RN[i], vol, tip_target(r) - 0.02 * li), NSEG + 1)
        S.append(P)
    layers.append(dict(S=np.array(S), roots=ROOTS[idx], width=width, cols=cols, vol=vol))
    log(f"layer {li}: {len(idx)} strands grown")

def out_normals(S):
    """per-point outward direction: body surface normal (stable for hair lying on the back/shoulders)"""
    O = np.zeros_like(S)
    for i in range(S.shape[0]):
        for k in range(S.shape[1]):
            loc, nn, _, _ = bvh.find_nearest(tuple(S[i, k]))
            O[i, k] = nn if nn is not None else (S[i, k] - Ch)
    return nrm(O)

# ------------------------------------------------------------------ clumps + waves (shared across layers so locks read as one)
from numpy.linalg import norm
allroots = np.concatenate([L_["roots"] for L_ in layers])
K = 140; cent = allroots[rng.choice(len(allroots), K, replace=False)]
for _ in range(12):
    lab = np.argmin(((allroots[:, None] - cent[None]) ** 2).sum(-1), 1)
    cent = np.array([allroots[lab == k].mean(0) if (lab == k).any() else cent[k] for k in range(K)])
phase = rng.uniform(0, 2 * np.pi, K); amp = rng.uniform(0.7, 1.3, K)
uN = np.linspace(0, 1, NSEG + 1)
off_i = 0
for L_ in layers:
    S = L_["S"]; lab = np.argmin(((L_["roots"][:, None] - cent[None]) ** 2).sum(-1), 1); L_["lab"] = lab
    guides = np.array([S[lab == k].mean(0) if (lab == k).any() else np.zeros((NSEG + 1, 3)) for k in range(K)])
    c = 0.62 * ss(0.12, 1.0, uN)[None, :, None]
    S = S + (guides[lab] - S) * c
    T = np.gradient(S, axis=1); T = nrm(T)
    out = out_normals(S); B = nrm(np.cross(T, out)); Nw = nrm(np.cross(B, T))
    arcl = np.concatenate([np.zeros((len(S), 1)), np.cumsum(norm(np.diff(S, axis=1), axis=2), 1)], 1)
    w = 2 * np.pi * arcl / 0.115 + phase[lab][:, None]
    A_ = (0.0085 * ss(0.12, 0.26, arcl) * amp[lab][:, None])[..., None]
    S = S + A_ * (np.sin(w)[..., None] * B + 0.55 * np.cos(w)[..., None] * Nw)
    for si in range(len(S)):                          # re-resolve collisions after waving
        for k in range(1, NSEG + 1):
            loc, nn, _, dist = bvh.find_nearest(tuple(S[si, k]))
            if loc is None: continue
            loc, nn = np.array(loc), np.array(nn); offv = 0.003 * L_["vol"] + 0.01 * L_["vol"] * ss(0, 0.1, arcl[si, k])
            if (S[si, k] - loc) @ nn < 0 or dist < offv: S[si, k] = loc + nn * offv
    L_["S"] = S
log("clumped + waved")

# ------------------------------------------------------------------ strand atlas (8 columns, roots at v=0)
TH, TW, NC = 2048, 2048, 8; CW = TW // NC
col = np.zeros((TH, TW), np.float32); alp = np.zeros((TH, TW), np.float32)
v = np.linspace(0, 1, TH)
for c in range(NC):
    dense = c < 4
    if dense:
        u = np.linspace(0, 1, CW)
        base = (ss(0.0, 0.25, u) * ss(1.0, 0.75, u))[None, :] * (0.34 * ss(0.0, 0.04, v) * ss(1.0, 0.75, v))[:, None]
        alp[:, c * CW:(c + 1) * CW] = np.maximum(alp[:, c * CW:(c + 1) * CW], base); col[:, c * CW:(c + 1) * CW] = 0.45
    for k in range(230 if dense else 150):
        x0 = rng.uniform(14, CW - 14); wpx = rng.uniform(0.9, 1.7); end = rng.uniform(0.62, 1.0)
        lum = rng.uniform(0.48, 0.80) if dense else rng.uniform(0.70, 1.05); wav = rng.uniform(0.5, 2.5); fq = rng.uniform(0.8, 2.4); ph = rng.uniform(0, 6.3)
        drift = rng.uniform(-6, 6)
        xs = x0 + wav * np.sin(2 * np.pi * fq * v + ph) + drift * v
        a_len = ss(0.0, 0.02, v) * ss(end, end - 0.18, v)
        shade = lum * (0.72 + 0.28 * ss(0.0, 0.35, v)) + 0.07 * ss(0.6, 1.0, v)
        for dxp in range(-3, 4):
            xi = np.clip(np.round(xs).astype(int) + dxp, 0, CW - 1)
            cov = np.clip(1 - np.abs(xi - xs) / wpx, 0, 1) * a_len
            gx = c * CW + xi; rows = np.arange(TH)
            a_old = alp[rows, gx]; a_new = a_old + cov * (1 - a_old)
            col[rows, gx] = np.where(a_new > 1e-4, (col[rows, gx] * a_old + shade * cov * (1 - a_old)) / np.maximum(a_new, 1e-4), col[rows, gx])
            alp[rows, gx] = a_new
img = np.dstack([col, col * 0.985, col * 0.97, np.clip(alp * 1.15, 0, 1)])
Image.fromarray(np.flipud(np.clip(img * 255 + 0.5, 0, 255).astype(np.uint8)), "RGBA").save(TEX + "/vivian_hair.png")
log("strand atlas painted")

# ------------------------------------------------------------------ physics chains (12 strand groups, 4 bones each + tip leaf)
NCH, CH_U = 12, np.array([0.22, 0.42, 0.62, 0.82, 1.0])          # chain joints along the normalized strand length
allS = np.concatenate([L_["S"] for L_ in layers]); allR = np.concatenate([L_["roots"] for L_ in layers])
feat = np.concatenate([allR, allS[:, -1] * 0.5], 1)                # root + tip position (tips decide where a lock hangs)
cc = feat[rng.choice(len(feat), NCH, replace=False)]
for _ in range(25):
    lab_c = np.argmin(((feat[:, None] - cc[None]) ** 2).sum(-1), 1)
    cc = np.array([feat[lab_c == k].mean(0) if (lab_c == k).any() else cc[k] for k in range(NCH)])
CHAINS = []
for k in range(NCH):
    G_ = allS[lab_c == k].mean(0)                                     # guide = mean member strand
    J = np.stack([np.interp(CH_U, uN, G_[:, d]) for d in range(3)], 1)
    CHAINS.append(J)
json.dump([J.tolist() for J in CHAINS], open(OUT + "/hair_chains.json", "w"))
# each strand binds to its two nearest chains (smooth seams), linearly along the chain by u
def strand_binding(root, tip):
    f = np.concatenate([root, tip * 0.5]); d = np.linalg.norm(cc - f, axis=1); o = np.argsort(d)[:2]
    w = 1.0 / np.maximum(d[o], 1e-4) ** 2; w /= w.sum(); return o, w
SEGS = "abcd"
def vert_weights(chs, cw, u):
    """{group: weight} for one card vertex at normalized length u"""
    out = {}
    head_w = float(ss(0.27, 0.17, u))
    if head_w > 0: out["head"] = head_w
    rest = 1.0 - head_w
    if rest <= 0: return out
    f = np.clip((u - CH_U[0]) / (CH_U[-1] - CH_U[0]), 0, 0.9999) * 4.0
    j = int(f); t = f - j
    for ch, w in zip(chs, cw):
        out[f"hair_{ch}_{SEGS[j]}"] = out.get(f"hair_{ch}_{SEGS[j]}", 0) + rest * w * (1 - t)
        if t > 0 and j + 1 < 4: out[f"hair_{ch}_{SEGS[j+1]}"] = out.get(f"hair_{ch}_{SEGS[j+1]}", 0) + rest * w * t
        elif t > 0: out[f"hair_{ch}_{SEGS[j]}"] += rest * w * t
    return out

# ------------------------------------------------------------------ card mesh
verts, faces, uvs, vw = [], [], [], []
for L_ in layers:
    S = L_["S"]; T = nrm(np.gradient(S, axis=1)); out = out_normals(S)
    for si in range(len(S)):
        P = S[si]; Bv = nrm(np.cross(T[si], out[si]))
        tw = np.radians(rng.normal(0, 22)) + np.radians(rng.normal(0, 10)) * uN          # per-card twist breaks the flat-ribbon look
        Bv = nrm(Bv * np.cos(tw)[:, None] + out[si] * np.sin(tw)[:, None])
        c = rng.choice(L_["cols"]); u0, u1 = (c + 0.04) / NC, (c + 0.96) / NC
        w = L_["width"] * (1 - 0.45 * uN) * rng.uniform(0.85, 1.15)
        chs, cw = strand_binding(P[0], P[-1])
        base = len(verts)
        for k in range(NSEG + 1):
            verts += [P[k] - Bv[k] * w[k] / 2, P[k] + Bv[k] * w[k] / 2]
            uvs += [(u0, uN[k]), (u1, uN[k])]
            wk = vert_weights(chs, cw, uN[k]); vw += [wk, wk]
        for k in range(NSEG):
            a = base + 2 * k; faces.append((a, a + 1, a + 3, a + 2))
mesh = bpy.data.meshes.new("vivian_hair"); mesh.from_pydata([tuple(x) for x in verts], [], faces); mesh.update()
uvl = mesh.uv_layers.new(name="UVMap")
loop_uv = np.array([uvs[li.vertex_index] for li in mesh.loops], np.float32); uvl.data.foreach_set("uv", loop_uv.ravel())
for p in mesh.polygons: p.use_smooth = True
hair = bpy.data.objects.new("vivian_hair", mesh); bpy.context.scene.collection.objects.link(hair)
groups = {}
for i, wk in enumerate(vw):
    for g, w in wk.items():
        if w < 1e-3: continue
        vg = groups.get(g) or hair.vertex_groups.new(name=g); groups[g] = vg
        vg.add([i], float(w), "REPLACE")
hair.parent = rig; am = hair.modifiers.new("Armature", "ARMATURE"); am.object = rig
am.use_deform_preserve_volume = True   # dual quaternion, as the engine skins (mesh.vert)
log(f"hair mesh: {len(faces)} card quads ({2*len(faces)} tris), {len(verts)} verts, {NCH} physics chains")

# material: neutral strands x tint, alpha clip
def hex_lin(h):
    c = np.array([int(h[i:i + 2], 16) for i in (1, 3, 5)]) / 255.0
    return tuple(np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4))
m = bpy.data.materials.new("vivian_hair"); m.use_nodes = True; nt = m.node_tree; nt.nodes.clear()
o_ = nt.nodes.new("ShaderNodeOutputMaterial"); bs = nt.nodes.new("ShaderNodeBsdfPrincipled"); nt.links.new(bs.outputs[0], o_.inputs[0])
ti = nt.nodes.new("ShaderNodeTexImage"); ti.image = bpy.data.images.load(TEX + "/vivian_hair.png")
mx = nt.nodes.new("ShaderNodeMix"); mx.data_type = "RGBA"; mx.blend_type = "MULTIPLY"; mx.inputs["Factor"].default_value = 1
mx.inputs[7].default_value = (*hex_lin(HAIR_HEX), 1); mx.name = "HAIR_TINT"
nt.links.new(ti.outputs[0], mx.inputs[6]); nt.links.new(mx.outputs[2], bs.inputs["Base Color"]); nt.links.new(ti.outputs[1], bs.inputs["Alpha"])
bs.inputs["Roughness"].default_value = 0.58; bs.inputs["Specular IOR Level"].default_value = 0.32
bs.inputs["Coat Weight"].default_value = 0.0
m.blend_method = "HASHED"; m.use_backface_culling = False
hair.data.materials.append(m)

# ------------------------------------------------------------------ scalp paint under the hair, all tones
z = np.load(OUT + ("/bakecache5.npz" if os.path.exists(OUT + "/bakecache5.npz") else "/bakecache4.npz")); POS, COV, MH, HP = z["POS"], z["COV"], z["MH"], z["HP"]
hz = hairline_z(POS); phi = np.abs(phi_of(POS))
ed = np.minimum(np.linalg.norm(POS - EARC[0], axis=-1), np.linalg.norm(POS - EARC[1], axis=-1))
scalp = COV & (MH > 0.5) & (ed > 0.02)
feather = ss(-0.004, 0.006, POS[..., 2] - hz) * scalp
strand = 0.55 + 0.45 * ss(0.3, 0.7, HP[..., 1])
hl = np.array(hex_lin(HAIR_HEX))
for tone in ("honey", "caramel", "deep"):
    p = f"{TEX}/vivian_skin_{tone}.jpg"
    a = np.flipud(np.asarray(Image.open(p).convert("RGB"), np.float32) / 255.0)
    lin = np.where(a <= 0.04045, a / 12.92, ((a + 0.055) / 1.055) ** 2.4)
    tgt = hl[None, None] * strand[..., None] * 0.9
    lin = lin + (tgt - lin) * (feather * 0.92)[..., None]
    a = np.where(lin <= 0.0031308, lin * 12.92, 1.055 * np.clip(lin, 0, 1) ** (1 / 2.4) - 0.055)
    Image.fromarray(np.flipud(np.clip(a * 255 + 0.5, 0, 255).astype(np.uint8))).save(p, quality=92)
for img in bpy.data.images:
    if img.filepath and "vivian_skin_" in img.filepath: img.reload()
log(f"scalp painted under the hair ({time.time()-t0:.0f}s)")
bpy.ops.wm.save_as_mainfile(filepath=OUT + "/vivian_s5.blend")
log("stage5 (hair) saved")
