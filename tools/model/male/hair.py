# Marcus hair v2: short natural men's cut as a fitted hair SHELL (offset scalp surface) with a strand-flow texture
# radiating from the crown, tapered thickness (fuller top, short sides/nape), a broken-up hairline, plus a sparse layer of
# very short cards on top for a soft silhouette. Replaces the spiky card cut.
#   blender -b out/marcus_c.blend --python male_hair.py
import bpy, bmesh, os, math, json, time, shlex
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from PIL import Image
M = "/root/agb/male"; OUT = M + "/out"; TEX = M + "/tex"
HAIR_HEX = "#24170f"
def prog(*a): os.system("python3 /root/agb/progress.py " + " ".join(shlex.quote(str(x)) for x in a))
def log(*a): print("[male]", *a, flush=True); prog("log", "[M] " + " ".join(str(x) for x in a))
rng = np.random.default_rng(9); t0 = time.time()
Lm = json.load(open(OUT + "/landmarks.json")); EL, ER = np.array(Lm["EL"]), np.array(Lm["ER"]); EC = (EL + ER) / 2
body = bpy.data.objects["Human"]; rig = bpy.data.objects["Human.rig"]
for kb in body.data.shape_keys.key_blocks: kb.value = 0
def ss(e0, e1, x):
    t = np.clip((np.asarray(x, np.float64) - e0) / (e1 - e0), 0, 1); return t * t * (3 - 2 * t)
mods = [(m, m.show_viewport) for m in body.modifiers]
for m, _ in mods: m.show_viewport = False
deps = bpy.context.evaluated_depsgraph_get(); me = body.evaluated_get(deps).to_mesh()
V = np.empty(len(me.vertices) * 3, np.float32); me.vertices.foreach_get("co", V); V = V.reshape(-1, 3)
VN = np.empty(len(me.vertices) * 3, np.float32); me.vertices.foreach_get("normal", VN); VN = VN.reshape(-1, 3)
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
wh, we = vgw("head"), vgw("ears")
Ch = np.array([0.0, EC[1] + 0.078, EC[2] + 0.012])
EARC = [V[(we > 0.5) & (np.sign(V[:, 0]) == s)].mean(0) for s in (1, -1)]
def phi_of(p): return np.arctan2(p[..., 0] - Ch[0], -(p[..., 1] - Ch[1]))
HLA = [0.0, 0.35, 0.70, 1.00, 1.25, 1.45, 1.75, 2.05, 2.50, math.pi]
HLZ = [0.076, 0.072, 0.064, 0.044, 0.028, 0.018, 0.004, -0.040, -0.068, -0.076]
def hl_margin(p):                                   # signed height above the hairline (m), ears carved out
    m = p[..., 2] - (EC[2] + np.interp(np.abs(phi_of(p)), HLA, HLZ))
    ear = np.minimum(np.linalg.norm(p - EARC[0], axis=-1), np.linalg.norm(p - EARC[1], axis=-1))
    return np.minimum(m, ear - 0.026)
marg = hl_margin(V)
sel = (wh > 0.5) & (marg > -0.004)                 # a little past the hairline so the fringe can feather out

# ------------------------------------------------------------------ shell mesh: scalp faces offset along the normal
faces = [list(p.vertices) for p in body.data.polygons if all(sel[i] for i in p.vertices)]
used = sorted({i for f in faces for i in f}); remap = {o: n for n, o in enumerate(used)}
P = V[used]; N = VN[used] / np.maximum(np.linalg.norm(VN[used], axis=1, keepdims=True), 1e-9)
top = ss(EC[2] + 0.05, EC[2] + 0.11, P[:, 2]); back = ss(1.9, 2.8, np.abs(phi_of(P)))
thick = (0.0028 + 0.0062 * top - 0.0012 * back * (1 - top)) * ss(-0.004, 0.012, marg[used])   # fuller top, tight sides, thin at the edge
P2 = P + N * (thick[:, None] + 0.0006)
# UV: strand flow radiating from the crown (v = polar angle from the crown, u = azimuth around it)
crown = Ch + np.array([0.0, 0.018, 0.105])
d = P2 - crown; up = np.array([0.0, 0.0, 1.0])
pol = np.arccos(np.clip((d / np.linalg.norm(d, axis=1, keepdims=True)) @ up, -1, 1))
az = np.arctan2(d[:, 0], -d[:, 1])
uvs = np.stack([(az / (2 * np.pi) + 0.5) * 6.0, pol / np.pi * 3.2], 1)   # 6 repeats around, strands ~ along v
mesh = bpy.data.meshes.new("marcus_hair")
mesh.from_pydata([tuple(x) for x in P2], [], [[remap[i] for i in f] for f in faces]); mesh.update()
uvl = mesh.uv_layers.new(name="UVMap")
lu = np.array([uvs[l.vertex_index] for l in mesh.loops], np.float32)
# fix the azimuth seam (u wraps behind the head): unwrap per face
for poly in mesh.polygons:
    idx = list(poly.loop_indices); us = lu[idx, 0]
    if us.max() - us.min() > 3.0: lu[idx, 0] = np.where(us < 3.0, us + 6.0, us)
uvl.data.foreach_set("uv", lu.ravel())
for p in mesh.polygons: p.use_smooth = True
# per-vertex fringe factor -> second UV channel is not exported reliably, so bake it into the alpha via a vertex attribute
edge = ss(-0.004, 0.010, marg[used])
col = mesh.color_attributes.new("fringe", "FLOAT_COLOR", "POINT")   # RGB 1 (albedo untouched), A = hairline fringe
c = np.ones((len(P2), 4), np.float32); c[:, 3] = edge; col.data.foreach_set("color", c.ravel())
mesh.color_attributes.active_color = col
log(f"hair shell: {len(faces)} faces, thickness top {thick.max()*1000:.1f} mm")

# ------------------------------------------------------------------ strand-flow texture (tileable), alpha carries the fringe pattern
TH, TW = 1024, 1024
img = np.zeros((TH, TW), np.float32); alp = np.zeros((TH, TW), np.float32)
yy = np.arange(TH)[:, None]
for k in range(2600):
    x0 = rng.uniform(0, TW); L = rng.uniform(80, 260); y0 = rng.uniform(0, TH)
    w = rng.uniform(0.8, 1.6); lum = rng.uniform(0.45, 1.0); drift = rng.uniform(-10, 10)
    ys = np.arange(int(L)); xs = (x0 + drift * ys / L + 1.2 * np.sin(ys / L * 3 + rng.uniform(0, 6))) % TW
    a = np.sin(np.pi * ys / L) ** 0.6
    for dx in (-1, 0, 1):
        xi = (np.round(xs).astype(int) + dx) % TW; yi = (int(y0) + ys) % TH
        cov = np.clip(1 - np.abs(xi - xs) / w, 0, 1) * a
        img[yi, xi] = np.maximum(img[yi, xi], lum * cov); alp[yi, xi] = np.maximum(alp[yi, xi], cov)
base = 0.42 + 0.1 * rng.random((TH, TW)).astype(np.float32)
lumi = base * (1 - alp) + img * alp
A = np.clip(0.55 + 0.45 * alp, 0, 1)                         # dense underlayer, strands punch the fringe
rgb = np.stack([lumi, lumi * 0.985, lumi * 0.97], -1)
Image.fromarray(np.clip(np.dstack([rgb, A]) * 255 + 0.5, 0, 255).astype(np.uint8), "RGBA").save(TEX + "/marcus_hair.png")
# strand normal map (grooves across the flow)
h = img
gx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 0.5
n = np.stack([-gx * 1.2, np.zeros_like(gx), np.ones_like(gx)], -1); n /= np.linalg.norm(n, axis=-1, keepdims=True)
Image.fromarray(np.clip((n * 0.5 + 0.5) * 255, 0, 255).astype(np.uint8)).save(TEX + "/marcus_hair_n.png")

hair = bpy.data.objects.new("marcus_hair", mesh); bpy.context.scene.collection.objects.link(hair)
g = hair.vertex_groups.new(name="head"); g.add(list(range(len(P2))), 1.0, "REPLACE")
hair.parent = rig; am = hair.modifiers.new("Armature", "ARMATURE"); am.object = rig
def hex_lin(hx):
    cc = np.array([int(hx[i:i + 2], 16) for i in (1, 3, 5)]) / 255.0
    return tuple(np.where(cc <= 0.04045, cc / 12.92, ((cc + 0.055) / 1.055) ** 2.4))
m = bpy.data.materials.new("marcus_hair"); m.use_nodes = True; nt = m.node_tree; nt.nodes.clear()
o_ = nt.nodes.new("ShaderNodeOutputMaterial"); bs = nt.nodes.new("ShaderNodeBsdfPrincipled"); nt.links.new(bs.outputs[0], o_.inputs[0])
ti = nt.nodes.new("ShaderNodeTexImage"); ti.image = bpy.data.images.load(TEX + "/marcus_hair.png")
mx = nt.nodes.new("ShaderNodeMix"); mx.data_type = "RGBA"; mx.blend_type = "MULTIPLY"; mx.inputs["Factor"].default_value = 1
mx.inputs[7].default_value = (*hex_lin(HAIR_HEX), 1); nt.links.new(ti.outputs[0], mx.inputs[6]); nt.links.new(mx.outputs[2], bs.inputs["Base Color"])
# alpha = texture alpha x fringe (vertex) -> clipped in game; Cycles preview uses the same product
fa = nt.nodes.new("ShaderNodeAttribute"); fa.attribute_name = "fringe"
mul = nt.nodes.new("ShaderNodeMath"); mul.operation = "MULTIPLY"
nt.links.new(ti.outputs[1], mul.inputs[0]); nt.links.new(fa.outputs["Alpha"], mul.inputs[1]); nt.links.new(mul.outputs[0], bs.inputs["Alpha"])
tn = nt.nodes.new("ShaderNodeTexImage"); tn.image = bpy.data.images.load(TEX + "/marcus_hair_n.png"); tn.image.colorspace_settings.name = "Non-Color"
nm = nt.nodes.new("ShaderNodeNormalMap"); nt.links.new(tn.outputs[0], nm.inputs["Color"]); nt.links.new(nm.outputs[0], bs.inputs["Normal"])
bs.inputs["Roughness"].default_value = 0.55; bs.inputs["Specular IOR Level"].default_value = 0.35
m.blend_method = "HASHED"; m.use_backface_culling = True; hair.data.materials.append(m)
log(f"hair v2 shell done ({time.time()-t0:.0f}s)")
bpy.ops.wm.save_as_mainfile(filepath=OUT + "/marcus_d.blend")
