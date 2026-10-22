# Marcus hair: short textured side-part crop — lifted top, tapered sides, short nape. Procedural cards on the real scalp.
#   blender -b out/marcus_c.blend --python male_hair.py
import bpy, os, math, json, time, shlex
import numpy as np
from mathutils.bvhtree import BVHTree
from PIL import Image
M = "/root/agb/male"; OUT = M + "/out"; TEX = M + "/tex"
HAIR_HEX = "#2a1b13"
def prog(*a): os.system("python3 /root/agb/progress.py " + " ".join(shlex.quote(str(x)) for x in a))
def log(*a): print("[male]", *a, flush=True); prog("log", "[male] " + " ".join(str(x) for x in a))
rng = np.random.default_rng(9); t0 = time.time()
Lm = json.load(open(OUT + "/landmarks.json")); EL, ER = np.array(Lm["EL"]), np.array(Lm["ER"]); EC = (EL + ER) / 2
body = bpy.data.objects["Human"]; rig = bpy.data.objects["Human.rig"]
for kb in body.data.shape_keys.key_blocks: kb.value = 0
def ss(e0, e1, x):
    t = np.clip((np.asarray(x, np.float64) - e0) / (e1 - e0), 0, 1); return t * t * (3 - 2 * t)
def nrm(v):
    n = np.linalg.norm(v, axis=-1, keepdims=True); return v / np.maximum(n, 1e-9)
mods = [(m, m.show_viewport) for m in body.modifiers]
for m, _ in mods: m.show_viewport = False
deps = bpy.context.evaluated_depsgraph_get(); me = body.evaluated_get(deps).to_mesh()
V = np.empty(len(me.vertices) * 3, np.float32); me.vertices.foreach_get("co", V); V = V.reshape(-1, 3); body.evaluated_get(deps).to_mesh_clear()
for m, s in mods: m.show_viewport = s
def vgw(name):
    w = np.zeros(len(body.data.vertices), np.float32)
    if name not in body.vertex_groups: return w
    gi = body.vertex_groups[name].index
    for v in body.data.vertices:
        for g in v.groups:
            if g.group == gi: w[v.index] = g.weight
    return w
wh, we = vgw("head"), vgw("ears")
bvh = BVHTree.FromPolygons([tuple(v) for v in V], [tuple(p.vertices) for p in body.data.polygons])
Ch = np.array([0.0, EC[1] + 0.078, EC[2] + 0.012])
EARC = [V[(we > 0.5) & (np.sign(V[:, 0]) == s)].mean(0) for s in (1, -1)]
def phi_of(p): return np.arctan2(p[..., 0] - Ch[0], -(p[..., 1] - Ch[1]))
HLA = [0.0, 0.35, 0.70, 1.00, 1.25, 1.45, 1.75, 2.05, 2.50, math.pi]
HLZ = [0.076, 0.072, 0.064, 0.044, 0.028, 0.018, 0.004, -0.040, -0.068, -0.076]
def hairline_z(p): return EC[2] + np.interp(np.abs(phi_of(p)), HLA, HLZ)
def on_scalp(p):
    far = np.minimum(np.linalg.norm(p - EARC[0], axis=-1), np.linalg.norm(p - EARC[1], axis=-1)) > 0.026
    return (p[..., 2] > hairline_z(p)) & far
sv = (wh > 0.5) & on_scalp(V) & (we < 0.2)
tris = []
for p in body.data.polygons:
    vs = list(p.vertices)
    if all(sv[i] for i in vs):
        tris.append((vs[0], vs[1], vs[2]))
        if len(vs) == 4: tris.append((vs[0], vs[2], vs[3]))
tris = np.array(tris); A, B, C = V[tris[:, 0]], V[tris[:, 1]], V[tris[:, 2]]
area = 0.5 * np.linalg.norm(np.cross(B - A, C - A), axis=1)
pick = rng.choice(len(tris), size=50000, p=area / area.sum()); r1, r2 = np.sqrt(rng.random(50000)), rng.random(50000)
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
ROOTS = poisson(cand, 0.0052)
RN = np.array([bvh.find_nearest(tuple(p))[1] for p in ROOTS])
log(f"male scalp {area.sum()*1e4:.0f} cm2, {len(ROOTS)} roots")
PART_X = 0.032                                      # part on his left; the top is combed across to his right and back
EX, EY, EZ = np.eye(3)
NSEG = 6
def grow(root, n):
    ph = abs(phi_of(root)); top = ss(EC[2] + 0.07, EC[2] + 0.11, root[2])
    side = 1.0 - top
    L = (0.022 + 0.040 * top) * (0.8 + 0.4 * rng.random()) * (0.7 if ph > 2.3 else 1.0)
    across = -EX * (1.0 if root[0] < PART_X else -0.6)
    desire = across * 0.55 * top + EY * (0.9 * top + 0.4 * side) - EZ * (0.15 * top + 0.9 * side)
    if ph < 0.6: desire += EZ * 0.15 + EY * 0.45                    # front swept up and back
    f = desire - n * (desire @ n); f /= max(1e-9, np.linalg.norm(f))
    lift = 0.22 * top + 0.08
    d = f * (1 - lift) + n * lift; d /= np.linalg.norm(d)
    p = root + n * 0.001; pts = [p.copy()]
    seg = L / NSEG
    for k in range(NSEG):
        d = d * 0.6 + f * 0.4 + (-EZ) * 0.05 * side; d /= np.linalg.norm(d)
        p = p + d * seg
        loc, nn, _, dist = bvh.find_nearest(tuple(p))
        off = 0.0025 + 0.006 * top * (k + 1) / NSEG
        if loc is not None:
            loc, nn = np.array(loc), np.array(nn)
            if (p - loc) @ nn < 0 or dist < off: p = loc + nn * off
        pts.append(p.copy())
    return np.array(pts)
S = np.array([grow(r, n) for r, n in zip(ROOTS, RN)])
log("strands grown")
# atlas: short dense strands (roots at v=0)
TH, TW, NC = 512, 1024, 4; CW = TW // NC
col = np.zeros((TH, TW), np.float32); alp = np.zeros((TH, TW), np.float32); v = np.linspace(0, 1, TH)
for c in range(NC):
    u = np.linspace(0, 1, CW)
    base = (ss(0.0, 0.2, u) * ss(1.0, 0.8, u))[None, :] * (0.45 * ss(0.0, 0.05, v) * ss(1.0, 0.65, v))[:, None]
    alp[:, c * CW:(c + 1) * CW] = base; col[:, c * CW:(c + 1) * CW] = 0.5
    for k in range(170):
        x0 = rng.uniform(10, CW - 10); wpx = rng.uniform(1.0, 1.9); end = rng.uniform(0.55, 1.0)
        lum = rng.uniform(0.55, 1.0); xs = x0 + rng.uniform(-8, 8) * v + rng.uniform(0, 1.5) * np.sin(v * 6 + rng.uniform(0, 6))
        a_len = ss(0.0, 0.03, v) * ss(end, end - 0.25, v)
        for dxp in range(-3, 4):
            xi = np.clip(np.round(xs).astype(int) + dxp, 0, CW - 1); cov = np.clip(1 - np.abs(xi - xs) / wpx, 0, 1) * a_len
            gx = c * CW + xi; rows = np.arange(TH); a_old = alp[rows, gx]; a_new = a_old + cov * (1 - a_old)
            col[rows, gx] = np.where(a_new > 1e-4, (col[rows, gx] * a_old + lum * cov * (1 - a_old)) / np.maximum(a_new, 1e-4), col[rows, gx]); alp[rows, gx] = a_new
img = np.dstack([col, col * 0.985, col * 0.97, np.clip(alp * 1.2, 0, 1)])
Image.fromarray(np.flipud(np.clip(img * 255 + 0.5, 0, 255).astype(np.uint8)), "RGBA").save(TEX + "/marcus_hair.png")
# cards
T = nrm(np.gradient(S, axis=1))
O = np.zeros_like(S)
for i in range(S.shape[0]):
    for k in range(S.shape[1]):
        _, nn, _, _ = bvh.find_nearest(tuple(S[i, k])); O[i, k] = nn if nn is not None else (S[i, k] - Ch)
O = nrm(O)
uN = np.linspace(0, 1, NSEG + 1)
verts, faces, uvs = [], [], []
for si in range(len(S)):
    P = S[si]; Bv = nrm(np.cross(T[si], O[si])); c = rng.integers(0, NC); u0, u1 = (c + 0.05) / NC, (c + 0.95) / NC
    w = 0.0115 * (1 - 0.35 * uN) * rng.uniform(0.85, 1.2); base = len(verts)
    for k in range(NSEG + 1):
        verts += [P[k] - Bv[k] * w[k] / 2, P[k] + Bv[k] * w[k] / 2]; uvs += [(u0, uN[k]), (u1, uN[k])]
    for k in range(NSEG):
        a = base + 2 * k; faces.append((a, a + 1, a + 3, a + 2))
mesh = bpy.data.meshes.new("marcus_hair"); mesh.from_pydata([tuple(x) for x in verts], [], faces); mesh.update()
uvl = mesh.uv_layers.new(name="UVMap"); uvl.data.foreach_set("uv", np.array([uvs[l.vertex_index] for l in mesh.loops], np.float32).ravel())
for p in mesh.polygons: p.use_smooth = True
hair = bpy.data.objects.new("marcus_hair", mesh); bpy.context.scene.collection.objects.link(hair)
g = hair.vertex_groups.new(name="head"); g.add(list(range(len(verts))), 1.0, "REPLACE")
hair.parent = rig; am = hair.modifiers.new("Armature", "ARMATURE"); am.object = rig
def hex_lin(h):
    c = np.array([int(h[i:i + 2], 16) for i in (1, 3, 5)]) / 255.0
    return tuple(np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4))
m = bpy.data.materials.new("marcus_hair"); m.use_nodes = True; nt = m.node_tree; nt.nodes.clear()
o_ = nt.nodes.new("ShaderNodeOutputMaterial"); bs = nt.nodes.new("ShaderNodeBsdfPrincipled"); nt.links.new(bs.outputs[0], o_.inputs[0])
ti = nt.nodes.new("ShaderNodeTexImage"); ti.image = bpy.data.images.load(TEX + "/marcus_hair.png")
mx = nt.nodes.new("ShaderNodeMix"); mx.data_type = "RGBA"; mx.blend_type = "MULTIPLY"; mx.inputs["Factor"].default_value = 1
mx.inputs[7].default_value = (*hex_lin(HAIR_HEX), 1); nt.links.new(ti.outputs[0], mx.inputs[6]); nt.links.new(mx.outputs[2], bs.inputs["Base Color"])
nt.links.new(ti.outputs[1], bs.inputs["Alpha"]); bs.inputs["Roughness"].default_value = 0.62; bs.inputs["Specular IOR Level"].default_value = 0.3
m.blend_method = "HASHED"; m.use_backface_culling = False; hair.data.materials.append(m)
log(f"hair: {len(faces)} quads, {len(verts)} verts ({time.time()-t0:.0f}s)")
bpy.ops.wm.save_as_mainfile(filepath=OUT + "/marcus_d.blend")
