# Vivian stage 5: adult woman (~42, mixed heritage), expression morphs carried to all face parts,
# anatomical vulva + vaginal/anal canals (vulva.py), and a 4K skin re-authored in three realistic tones
# (honey / caramel / deep) that share one normal + ORM map.
#   blender -b --python build5.py          REUSE=1 reuses cached bakes (same geometry only)
# Output keeps the stage-4 file name (out/vivian_s4.blend) so hair.py / rig_export.py run unchanged.
import bpy, os, sys, math, time, shlex, json, glob
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from mathutils.geometry import barycentric_transform, intersect_point_tri
from bl_ext.user_default.mpfb.services.humanservice import HumanService
from bl_ext.user_default.mpfb.services.assetservice import AssetService
from bl_ext.user_default.mpfb.services.targetservice import TargetService
from bl_ext.user_default.mpfb.entities.clothes.mhclo import Mhclo
from PIL import Image

OUT = "/root/agb/out"; TEX = OUT + "/tex"
os.makedirs(TEX, exist_ok=True)
RES = 4096
MPFB = "/home/marimo/.config/blender/4.2/extensions/user_default/mpfb/data/targets"
SKINS = "/root/agb/mh_assets/skins"
SRC = {"cau": SKINS + "/middleage_caucasian_female/middleage_lightskinned_female_diffuse.png",
       "afr": SKINS + "/middleage_african_female/middleage_darkskinned_female_diffuse.png"}
DEFAULT_TONE = "caramel"
def prog(*a): os.system("python3 /root/agb/progress.py " + " ".join(shlex.quote(str(x)) for x in a))
def log(*a): print("[build5]", *a, flush=True); prog("log", "[F] " + " ".join(str(x) for x in a))
t0 = time.time()

# ================================================================== human
bpy.ops.wm.read_factory_settings(use_empty=True)
info = HumanService._create_default_human_info_dict()
info["phenotype"] = {
    "gender": 0.0, "age": 0.63,                     # MakeHuman age 0.63 ~ 42 years
    "muscle": 0.52, "weight": 0.62, "proportions": 0.88, "height": 0.6,
    "cupsize": 0.84, "firmness": 0.52,
    "race": {"caucasian": 0.5, "african": 0.38, "asian": 0.12},
}
targets = {
    "hip-scale-horiz-incr": 0.32, "buttocks-volume-incr": 0.6, "measure-waist-circ-decr": 0.5,
    "measure-hips-circ-incr": 0.3, "stomach-pregnant-decr": 0.15, "measure-thigh-circ-incr": 0.18,
    "nipple-point-incr": 0.3, "nipple-size-incr": 0.2,
    "mouth-upperlip-volume-incr": 0.35, "mouth-lowerlip-volume-incr": 0.45, "mouth-cupidsbow-incr": 0.3,
    "l-cheek-bones-incr": 0.35, "r-cheek-bones-incr": 0.35, "l-cheek-volume-decr": 0.25, "r-cheek-volume-decr": 0.25,
    "l-eye-corner2-up": 0.25, "r-eye-corner2-up": 0.25, "l-eye-scale-incr": 0.12, "r-eye-scale-incr": 0.12,
    "eyebrows-angle-up": 0.2, "chin-prominent-incr": 0.12, "chin-width-decr": 0.15, "head-oval": 0.4,
    "head-fat-decr": 0.2, "nose-scale-horiz-decr": 0.18, "nose-point-width-decr": 0.25, "nose-hump-decr": 0.3,
    "nose-nostrils-width-decr": 0.1, "nose-volume-decr": 0.1, "forehead-scale-vert-decr": 0.12,
}
known = {os.path.basename(p)[:-10] for p in glob.glob(MPFB + "/**/*.target.gz", recursive=True)}
info["targets"] = [{"target": t, "value": v} for t, v in targets.items() if t in known]
info["skin_mhmat"] = "middleage_caucasian_female/middleage_caucasian_female.mhmat"
info["skin_material_type"] = "GAMEENGINE"
settings = HumanService.get_default_deserialization_settings(); settings["subdiv_levels"] = 0
body = HumanService.deserialize_from_dict(info, settings)
rig = HumanService.add_builtin_rig(body, "game_engine_with_breast", import_weights=True)
PARTS = {}
for sub, fn in [("eyes", "high-poly/high-poly.mhclo"), ("eyebrows", "eyebrow010/eyebrow010.mhclo"),
                ("eyelashes", "eyelashes03/eyelashes03.mhclo"), ("teeth", "teeth_base/teeth_base.mhclo"),
                ("tongue", "tongue01/tongue01.mhclo")]:
    p = AssetService.find_asset_absolute_path(fn, asset_subdir=sub)
    before = set(bpy.data.objects)
    HumanService.add_mhclo_asset(p, body, asset_type=sub, subdiv_levels=0, material_type="MAKESKIN")
    PARTS[sub] = [o for o in bpy.data.objects if o not in before and o.type == "MESH"][0]
    PARTS[sub]["mhclo_path"] = p
TargetService.bake_targets(body)
for o in PARTS.values():
    if o.data.shape_keys:
        o.shape_key_add(name="__mix", from_mix=True)
        for k in list(o.data.shape_keys.key_blocks): o.shape_key_remove(k)
log(f"human built ({len(info['targets'])} sculpt targets, mixed heritage) {time.time()-t0:.0f}s")
prog("set", "body", "done", "Age ~42, curvy hourglass figure, mixed heritage, refined face")

# ================================================================== expressions
def basis_co(o):
    a = np.empty(len(o.data.vertices) * 3, np.float32); o.data.vertices.foreach_get("co", a); return a.reshape(-1, 3)
B0 = basis_co(body)
body.shape_key_add(name="Basis", from_mix=False)
unit_delta = {}
for f in sorted(glob.glob(MPFB + "/expression/units/caucasian/*.target.gz")):
    name = os.path.basename(f)[:-10]
    TargetService.load_target(body, f, weight=0.0, name="u_" + name)
    kb = body.data.shape_keys.key_blocks["u_" + name]
    a = np.empty(len(kb.data) * 3, np.float32); kb.data.foreach_get("co", a)
    unit_delta[name] = a.reshape(-1, 3) - B0
for kb in list(body.data.shape_keys.key_blocks):
    if kb.name.startswith("u_"): body.shape_key_remove(kb)
U = lambda n, w: unit_delta[n] * w
EXPR = {
    "blink":    U("eye-left-closure", 1) + U("eye-right-closure", 1),
    "moan":     U("mouth-open", 0.55) + U("mouth-parling", 0.35) + U("eyebrows-left-inner-up", 0.45) + U("eyebrows-right-inner-up", 0.45)
                + U("eye-left-slit", 0.35) + U("eye-right-slit", 0.35),
    "pleasure": U("eyebrows-left-inner-up", 0.85) + U("eyebrows-right-inner-up", 0.85) + U("eye-left-slit", 0.6) + U("eye-right-slit", 0.6)
                + U("mouth-parling", 0.5) + U("mouth-open", 0.18),
    "oral":     U("mouth-open", 1.0) + U("mouth-pursing", 0.35) + U("eye-left-slit", 0.3) + U("eye-right-slit", 0.3),
    "kiss":     U("mouth-pursing", 0.8) + U("mouth-protusion", 0.55) + U("eye-left-closure", 0.75) + U("eye-right-closure", 0.75),
    "smile":    U("mouth-corner-puller", 0.75) + U("eye-left-slit", 0.22) + U("eye-right-slit", 0.22) + U("mouth-elevation", 0.2),
}
for name, d in EXPR.items():
    kb = body.shape_key_add(name=name, from_mix=False); kb.data.foreach_set("co", (B0 + d).ravel())
for sub, o in PARTS.items():
    m = Mhclo(); m.load(o["mhclo_path"])
    n = len(o.data.vertices); idx = np.zeros((n, 3), np.int64); w = np.zeros((n, 3), np.float32); ok = np.zeros(n, bool)
    for vi, mp in m.verts.items():
        if vi < n: idx[vi] = mp["verts"]; w[vi] = mp["weights"]; ok[vi] = True
    P0 = basis_co(o); o.shape_key_add(name="Basis", from_mix=False)
    for name, d in EXPR.items():
        off = (d[idx] * w[..., None]).sum(1) * ok[:, None]
        if np.abs(off).max() < 1e-4: continue
        kb = o.shape_key_add(name=name, from_mix=False); kb.data.foreach_set("co", (P0 + off).ravel())
log(f"expression morphs {list(EXPR)} on body + lashes/brows/teeth/tongue")

# ================================================================== body-shape variants -> body_* morphs
# Each variant is a separate MPFB base mesh (same topology) with one change; its delta from Vivian becomes a morph.
# Deltas are masked off the head/neck (face + hair stay put) and re-centred on the pelvis (skeleton stays seated).
import copy
VARIANTS = {
    # native: bidirectional pairs (slider -1..+1 -> _dn/_up), strong but natural adult ranges.
    # target values are ADDED to Vivian's own sculpt targets (negative undoes them).
    "bust_up":   ({"cupsize": 1.0, "firmness": 0.62}, {"breast-volume-vert-up": 0.45, "measure-bust-circ-incr": 0.55}),
    "bust_dn":   ({"cupsize": 0.22, "firmness": 0.62}, {"measure-bust-circ-decr": 0.35}),
    "hips_up":   ({}, {"hip-scale-horiz-incr": 0.45, "measure-hips-circ-incr": 0.45}),
    "hips_dn":   ({}, {"hip-scale-horiz-incr": -0.32, "hip-scale-horiz-decr": 0.3, "measure-hips-circ-incr": -0.3, "measure-hips-circ-decr": 0.3}),
    "waist_up":  ({}, {"measure-waist-circ-decr": -0.5, "measure-waist-circ-incr": 0.35}),
    "waist_dn":  ({}, {"measure-waist-circ-decr": 0.5}),
    "butt_up":   ({}, {"buttocks-volume-incr": 0.4}),
    "butt_dn":   ({}, {"buttocks-volume-incr": -0.6, "buttocks-volume-decr": 0.4}),
    "weight_up": ({"weight": 0.9}, {}),
    "weight_dn": ({"weight": 0.36}, {}),
    "muscle_up": ({"muscle": 0.88}, {}),
    "belly_up":  ({}, {"stomach-pregnant-incr": 0.4, "stomach-pregnant-decr": -0.15}),
}
def vg_w(o, name):
    w = np.zeros(len(o.data.vertices), np.float32)
    if name not in o.vertex_groups: return w
    gi = o.vertex_groups[name].index
    for v in o.data.vertices:
        for g in v.groups:
            if g.group == gi: w[v.index] = g.weight
    return w
keep = np.clip(1.0 - (vg_w(body, "head") + 0.8 * vg_w(body, "neck_01")), 0, 1)[:, None]
pel = (np.abs(B0[:, 0]) < 0.03) & (np.abs(B0[:, 2] - 0.93) < 0.04)
for vname, (pheno, tg) in VARIANTS.items():
    inf = HumanService._create_default_human_info_dict()
    inf["phenotype"] = copy.deepcopy(info["phenotype"]); inf["phenotype"].update(pheno)
    tt = dict(targets)
    for k_, v_ in tg.items(): tt[k_] = max(0.0, tt.get(k_, 0) + v_)
    inf["targets"] = [{"target": t_, "value": v_} for t_, v_ in tt.items() if t_ in known]
    vo = HumanService.deserialize_from_dict(inf, settings)
    TargetService.bake_targets(vo)
    D = basis_co(vo) - B0
    vme = vo.data; bpy.data.objects.remove(vo); bpy.data.meshes.remove(vme)
    D -= D[pel].mean(0)
    D *= keep
    kb = body.shape_key_add(name=vname, from_mix=False); kb.data.foreach_set("co", (B0 + D).ravel())
    log(f"shape morph {vname}: max {np.abs(D).max()*1000:.0f} mm")

# ================================================================== anatomy: vulva + canals (vulva.py, same folder)
def uv_coverage(me, R=1024):
    """texture-space coverage of the current UV layout (for finding free space for the canal UV islands)"""
    from PIL import ImageDraw
    im = Image.new("L", (R, R), 0); dr = ImageDraw.Draw(im); uvd = me.uv_layers.active.data
    for poly in me.polygons:
        pts = [(uvd[li].uv[0] * R, uvd[li].uv[1] * R) for li in poly.loop_indices]
        dr.polygon(pts, fill=255)
    a = np.asarray(im.resize((RES, RES), Image.NEAREST)) > 0
    return a                                   # row 0 = v 0 (same convention as the bakes)
_here = os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else "/root/agb"
exec(open(os.path.join(_here, "vulva.py")).read())
build_vulva(body, uv_coverage(body.data), log=log)

# ================================================================== geometry + landmarks
mask_mods = [m for m in body.modifiers if m.type == "MASK"]
def coords(o, deps):
    e = o.evaluated_get(deps); me = e.to_mesh()
    a = np.empty(len(me.vertices) * 3, np.float32); me.vertices.foreach_get("co", a); e.to_mesh_clear()
    return a.reshape(-1, 3) @ np.array(o.matrix_world)[:3, :3].T + np.array(o.matrix_world)[:3, 3]
for m in mask_mods: m.show_viewport = False
deps = bpy.context.evaluated_depsgraph_get(); V = coords(body, deps)
for m in mask_mods: m.show_viewport = True
def vg_weights(name, obj=body):
    w = np.zeros(len(obj.data.vertices), np.float32)
    if name not in obj.vertex_groups: return w
    gi = obj.vertex_groups[name].index
    for v in obj.data.vertices:
        for g in v.groups:
            if g.group == gi: w[v.index] = g.weight
    return w
wbody = vg_weights("body")
bvh_body = BVHTree.FromPolygons([tuple(v) for v in V], [tuple(p.vertices) for p in body.data.polygons if all(wbody[i] > 0.5 for i in p.vertices)])
browo = PARTS["eyebrows"]; BROW = coords(browo, deps)
bvh_brow = BVHTree.FromPolygons([tuple(p) for p in BROW], [tuple(p.vertices) for p in browo.data.polygons])
E = coords(PARTS["eyes"], deps); EL, ER = E[E[:, 0] > 0].mean(0), E[E[:, 0] < 0].mean(0)
LASH = coords(PARTS["eyelashes"], deps)
lash_d = np.array([(bvh_body.find_nearest(tuple(p))[3] or 1.0) for p in LASH])
def lash_roots(C, sgn):
    m = (lash_d < 0.0012) & (np.sign(LASH[:, 0]) == sgn) & (np.abs(LASH[:, 2] - C[2]) < 0.0065) & (np.abs(LASH[:, 0] - C[0]) < 0.017)
    return LASH[m]
TE = coords(PARTS["teeth"], deps); tz = TE[:, 2].min() + 0.6 * (TE[:, 2].max() - TE[:, 2].min())
cand = np.where((np.abs(V[:, 0]) < 0.006) & (np.abs(V[:, 2] - tz) < 0.012) & (V[:, 1] < TE[:, 1].min()) & (wbody > 0.5))[0]
MOUTH = V[cand[np.argmin(V[cand, 1])]]
def tip(w):
    idx = np.where((w > 0.6) & (wbody > 0.5))[0]; return V[idx[np.argmin(V[idx, 1])]]
NIP = [tip(vg_weights("breast_l")), tip(vg_weights("breast_r"))]
bones = {b.name: (np.array(rig.matrix_world @ b.head_local), np.array(rig.matrix_world @ b.tail_local)) for b in rig.data.bones}
log(f"landmarks: eyes {EL.round(3)} mouth {MOUTH.round(3)} nipples {NIP[0].round(3)}")

# ================================================================== bakes (cached)
sc = bpy.context.scene
sc.render.engine = "CYCLES"; sc.cycles.device = "CPU"; sc.cycles.samples = 1; sc.cycles.use_denoising = False
for o in bpy.context.view_layer.objects: o.select_set(False)
body.select_set(True); bpy.context.view_layer.objects.active = body
def add_attr(values, attr):
    a = body.data.color_attributes.new(attr, "FLOAT_COLOR", "POINT")
    c = np.zeros((len(values), 4), np.float32); c[:, :3] = values[:, None]; c[:, 3] = 1; a.data.foreach_set("color", c.ravel())
add_attr(vg_weights("fingernails"), "m_nails"); add_attr(vg_weights("genitals"), "m_gen"); add_attr(vg_weights("head"), "m_head")
mdz = np.clip(-unit_delta["mouth-open"][:, 2], 0, None)
mdz = np.concatenate([mdz, np.zeros(len(body.data.vertices) - len(mdz), np.float32)])   # new crotch verts never move
add_attr(mdz / max(1e-6, mdz.max()), "m_mdz")
def bake(name, build, margin=24, samples=1, btype="EMIT"):
    img = bpy.data.images.new(name, RES, RES, alpha=True, float_buffer=True); img.colorspace_settings.name = "Non-Color"
    mat = bpy.data.materials.new("BAKE_" + name); mat.use_nodes = True; nt = mat.node_tree; nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    if btype == "EMIT":
        em = nt.nodes.new("ShaderNodeEmission"); nt.links.new(em.outputs[0], out.inputs[0]); nt.links.new(build(nt), em.inputs["Color"])
    else:
        bs = nt.nodes.new("ShaderNodeBsdfDiffuse"); nt.links.new(bs.outputs[0], out.inputs[0])
    tex = nt.nodes.new("ShaderNodeTexImage"); tex.image = img; nt.nodes.active = tex
    saved = [s.material for s in body.material_slots]
    for s in body.material_slots: s.material = mat
    sc.cycles.samples = samples; sc.render.bake.margin = margin; sc.render.bake.margin_type = "EXTEND"
    t = time.time(); bpy.ops.object.bake(type=btype, use_clear=True); print(f"baked {name} in {time.time()-t:.1f}s", flush=True)
    for s, m in zip(body.material_slots, saved): s.material = m
    a = np.empty(RES * RES * 4, np.float32); img.pixels.foreach_get(a); bpy.data.images.remove(img)
    return a.reshape(RES, RES, 4)
def geo(sock): return lambda nt: nt.nodes.new("ShaderNodeNewGeometry").outputs[sock]
def attr(name):
    def f(nt):
        a = nt.nodes.new("ShaderNodeAttribute"); a.attribute_name = name; return a.outputs["Color"]
    return f
def one(nt):
    r = nt.nodes.new("ShaderNodeRGB"); r.outputs[0].default_value = (1, 1, 1, 1); return r.outputs[0]
def proc_height(nt):
    tc = nt.nodes.new("ShaderNodeTexCoord").outputs["Object"]
    def vor(scale):
        v = nt.nodes.new("ShaderNodeTexVoronoi"); v.inputs["Scale"].default_value = scale; nt.links.new(tc, v.inputs["Vector"]); return v.outputs["Distance"]
    def noi(scale, detail):
        n = nt.nodes.new("ShaderNodeTexNoise"); n.inputs["Scale"].default_value = scale; n.inputs["Detail"].default_value = detail
        nt.links.new(tc, n.inputs["Vector"]); return n.outputs["Fac"]
    cmb = nt.nodes.new("ShaderNodeCombineColor")
    nt.links.new(vor(900), cmb.inputs[0]); nt.links.new(noi(380, 8), cmb.inputs[1]); nt.links.new(noi(22, 4), cmb.inputs[2])
    return cmb.outputs[0]
CACHE = OUT + "/bakecache5.npz"
VMN = ("v_muc", "v_min", "v_maj", "v_clit", "v_anus", "v_pub", "v_cav")
KEYS = ("COV", "POS", "NRM", "MN", "MG", "MH", "MDZ", "HP", "AO", "VM")
if os.environ.get("REUSE") and os.path.exists(CACHE):
    z = np.load(CACHE); COV, POS, NRM, MN, MG, MH, MDZ, HP, AO, VM = (z[k] for k in KEYS); log("bakes loaded from cache")
else:
    COV = bake("cov", one, margin=0)[..., 0] > 0.5
    POS = bake("pos", geo("Position"))[..., :3]; NRM = bake("nrm", geo("Normal"))[..., :3]
    MN = bake("m_nails", attr("m_nails"))[..., 0]; MG = bake("m_gen", attr("m_gen"))[..., 0]; MH = bake("m_head", attr("m_head"))[..., 0]
    MDZ = bake("m_mdz", attr("m_mdz"))[..., 0]
    HP = bake("hproc", proc_height)[..., :3]; AO = bake("ao", None, samples=int(os.environ.get("AO_SAMPLES", "24")), btype="AO")[..., 0]
    VM = np.stack([bake(n, attr(n))[..., 0] for n in VMN], 0)
    np.savez(CACHE, COV=COV, POS=POS, NRM=NRM, MN=MN, MG=MG, MH=MH, MDZ=MDZ, HP=HP, AO=AO, VM=VM); log(f"4K bakes done ({time.time()-t0:.0f}s)")

# ================================================================== masks (tone independent)
def ss(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0, 1); return t * t * (3 - 2 * t)
def region(center, radius):
    d = np.linalg.norm(POS - center, axis=-1); return np.where((d < radius) & COV)
def nearest(points, P, chunk=4096):
    best = np.full(len(P), 9.0, np.float32); arg = np.zeros(len(P), np.int64)
    for i in range(0, len(P), chunk):
        d = np.linalg.norm(P[i:i + chunk, None, :] - points[None, :, :], axis=-1); arg[i:i + chunk] = d.argmin(1); best[i:i + chunk] = d.min(1)
    return best, arg
def blur(a, r):
    k = np.arange(-int(3 * r), int(3 * r) + 1); w = np.exp(-(k / r) ** 2 / 2); w /= w.sum()
    out = np.zeros_like(a, dtype=np.float32)
    for o, wk in zip(k, w): out += wk * np.roll(a, o, axis=0)
    res = np.zeros_like(out)
    for o, wk in zip(k, w): res += wk * np.roll(out, o, axis=1)
    return res
Z = lambda: np.zeros((RES, RES), np.float32)
M = {k: Z() for k in ("shadow", "liner", "smudge", "browfill", "lip", "wet", "blush", "nail", "areola", "nipple",
                      "flush", "knuckle", "freckle", "mole")}
H = Z(); ROUGH = np.full((RES, RES), 0.52, np.float32)
rng = np.random.default_rng(11)

# ---- eyes: crease shadow, winged liner, lower smudge
for C, sgn in ((EL, 1), (ER, -1)):
    roots = lash_roots(C, sgn)
    up = roots[roots[:, 2] > C[2] - 0.0005]; lo = roots[roots[:, 2] <= C[2] - 0.0005]
    yy, xx = region(C, 0.045); P = POS[yy, xx]
    if len(up) < 5: log("WARN few lash roots", len(up)); continue
    d_up, ia = nearest(up, P); lid = P[:, 2] >= up[ia, 2] - 0.0004
    d_br = np.array([(bvh_brow.find_nearest(tuple(p))[3] or 1.0) for p in P], np.float32)
    outer = np.clip((sgn * (P[:, 0] - C[0]) + 0.004) / 0.018, 0, 1)
    sh = (ss(0.0085, 0.0012, d_up) * (0.35 + 0.65 * outer) + 0.3 * np.exp(-((d_up - 0.0058) / 0.002) ** 2) * (0.25 + 0.75 * outer)) * lid
    sh *= ss(0.0045, 0.0095, d_br) * ss(0.0125, 0.0085, d_up)
    M["shadow"][yy, xx] = np.maximum(M["shadow"][yy, xx], np.clip(sh, 0, 1))
    oc = up[np.argmax(sgn * up[:, 0])]
    wdir = np.array([sgn * 0.84, 0.15, 0.52]); wdir /= np.linalg.norm(wdir)
    t = np.clip(((P - oc) @ wdir) / 0.0072, 0, 1)
    dseg = np.linalg.norm(P - (oc + wdir[None] * 0.0072 * t[:, None]), axis=-1)
    width = 0.0008 * (1 - t) + 0.0002
    wing = ss(width + 0.00035, width, dseg) * (t > 0.02)
    M["liner"][yy, xx] = np.maximum(M["liner"][yy, xx], np.clip(np.maximum(ss(0.0014, 0.0005, d_up) * lid * (0.7 + 0.3 * outer), wing), 0, 1))
    if len(lo) > 3:
        d_lo, ib = nearest(lo, P); below = P[:, 2] <= lo[ib, 2] + 0.0004
        M["smudge"][yy, xx] = np.maximum(M["smudge"][yy, xx], np.clip(ss(0.0022, 0.0005, d_lo) * below * (0.2 + 0.8 * outer), 0, 1))
log("eye masks")

# ---- brows: transfer the brow cards' hair alpha onto the skin underneath (hair-stroke underlay)
bimg = np.asarray(Image.open("/root/agb/mh_assets/eyebrows/eyebrow010/eyebrow010.png").convert("RGBA"), np.float32)[..., 3] / 255
bh, bw = bimg.shape
uvl = browo.data.uv_layers.active.data
polys = [(list(p.vertices), [tuple(uvl[li].uv) for li in p.loop_indices]) for p in browo.data.polygons]
for C in (EL, ER):
    yy, xx = region(C + np.array([0, 0, 0.02]), 0.04); P = POS[yy, xx]; val = np.zeros(len(P), np.float32)
    for i, p in enumerate(P):
        loc, nrm, pi, dist = bvh_brow.find_nearest(tuple(p))
        if pi is None or dist > 0.004: continue
        vs, uvs = polys[pi]; loc = Vector(loc)
        for tri in ((0, 1, 2), (0, 2, 3)) if len(vs) == 4 else ((0, 1, 2),):
            A = [Vector(BROW[vs[j]]) for j in tri]; Bv = [Vector((uvs[j][0], uvs[j][1], 0)) for j in tri]
            uv = barycentric_transform(loc, *A, *Bv)
            if intersect_point_tri(loc, *A) is not None or tri == (0, 2, 3):
                u, v = uv.x % 1.0, uv.y % 1.0
                val[i] = bimg[int((1 - v) * (bh - 1)), int(u * (bw - 1))] * ss(0.004, 0.0012, dist)
                break
    M["browfill"][yy, xx] = np.maximum(M["browfill"][yy, xx], val)
M["browfill"] = np.clip(blur(M["browfill"], 1.2) * 1.4, 0, 1) * COV
log("brow underlay transferred")

# ---- lips: the mouth line is where the mouth-open morph separates the still upper lip from the dropping lower lip
yy, xx = region(MOUTH, 0.05); P = POS[yy, xx]; N = NRM[yy, xx]; rel = P - MOUTH; md = MDZ[yy, xx]
front = N[:, 1] < -0.35
cols = np.round(rel[:, 0] / 0.0008).astype(int)
lx, lz, corner = [], [], 0.0
for c in np.unique(cols[front & (np.abs(rel[:, 0]) < 0.032)]):
    m = front & (cols == c) & (np.abs(rel[:, 2]) < 0.02)
    upz = rel[m & (md < 0.3), 2]; loz = rel[m & (md > 0.7), 2]
    if len(upz) > 2 and len(loz) > 2:
        a_, b_ = upz.min(), loz.max()
        if a_ - b_ < 0.004:
            lx.append(c * 0.0008); lz.append(0.5 * (a_ + b_)); corner = max(corner, abs(c * 0.0008))
lx = np.array(lx); lz = np.array(lz); o = np.argsort(lx); lx, lz = lx[o], lz[o]
lz = np.convolve(np.pad(lz, 3, mode="edge"), np.ones(7) / 7, mode="valid")
W = float(np.clip(corner, 0.021, 0.027))
xr = rel[:, 0]; d = rel[:, 2] - np.interp(xr, lx, lz); q = np.clip(np.abs(xr) / W, 0, 1)
hu = 0.0080 * (1 - q ** 2.2) ** 0.75 * (1 - 0.16 * np.exp(-(xr / 0.0036) ** 2))
hl = 0.0098 * (1 - q ** 2.0) ** 0.6
e = 0.0007
side = ss(W + 0.002, W - 0.003, np.abs(xr))
lipm = np.maximum(ss(-e, e, d) * ss(hu + e, hu - e, d), ss(e, -e, d) * ss(-hl - e, -hl + e, d)) * side
inside = (N[:, 1] > -0.2) & (P[:, 1] > MOUTH[1] + 0.0015) & (np.abs(d) < 0.006) & (np.abs(xr) < W - 0.002)
M["lip"][yy, xx] = np.maximum(lipm, inside * 0.9); M["wet"][yy, xx] = inside
M["lip"] = np.clip(blur(M["lip"], 1.0), 0, 1) * COV
log(f"lips: mouth line from {len(lx)} slices, half-width {W*1000:.1f} mm")

# ---- cheeks, nails, areolae, vulva, joints, freckles, beauty mark
for C, sgn in ((EL, 1), (ER, -1)):
    bc = C + np.array([sgn * 0.012, -0.012, -0.03]); yy, xx = region(bc, 0.032)
    M["blush"][yy, xx] = np.exp(-(np.linalg.norm(POS[yy, xx] - bc, axis=-1) / 0.017) ** 2) * (NRM[yy, xx, 1] < 0.2)
M["nail"] = ss(0.15, 0.75, np.clip(blur(ss(0.3, 0.75, MN), 2.0) * 1.15, 0, 1)) * COV
for T_ in NIP:
    yy, xx = region(T_, 0.035); P = POS[yy, xx]; dd = np.linalg.norm(P - T_, axis=-1)
    M["areola"][yy, xx] = ss(0.025, 0.017, dd); M["nipple"][yy, xx] = ss(0.0085, 0.0055, dd)
    bumps = np.zeros(len(P), np.float32)
    for k in range(16):
        a_ = rng.uniform(0, 2 * np.pi); r = rng.uniform(0.011, 0.019)
        bp = T_ + np.array([np.cos(a_) * r, 0.004, np.sin(a_) * r])
        bumps += np.exp(-(np.linalg.norm((P - bp)[:, [0, 2]], axis=-1) / 0.0009) ** 2)
    H[yy, xx] += 0.6 * bumps + 0.35 * M["nipple"][yy, xx] * (HP[yy, xx, 1] - 0.5); ROUGH[yy, xx] -= 0.05 * M["areola"][yy, xx]
# anatomy masks (vertex attributes from vulva.py, baked): mucosa, minora/majora pigment, clitoris, anus, canals, landing strip
for k_, n_ in enumerate(VMN): M[n_] = np.clip(VM[k_], 0, 1) * COV
hairs = ((HP[..., 0] < 0.16) | (HP[..., 1] > 0.66)).astype(np.float32)
M["v_pub"] = M["v_pub"] * (0.3 + 0.6 * hairs)
H += (HP[..., 1] - 0.5) * 0.55 * M["v_min"] + (HP[..., 1] - 0.5) * 0.3 * M["v_anus"]
ROUGH = ROUGH * (1 - M["v_muc"]) + 0.2 * M["v_muc"]
for bn, r, s_ in [("calf_l", 0.05, 0.22), ("calf_r", 0.05, 0.22), ("lowerarm_l", 0.035, 0.16), ("lowerarm_r", 0.035, 0.16),
                  ("foot_l", 0.04, 0.16), ("foot_r", 0.04, 0.16)]:
    c = bones[bn][0]; yy, xx = region(c, r * 1.8)
    M["flush"][yy, xx] = np.maximum(M["flush"][yy, xx], np.exp(-(np.linalg.norm(POS[yy, xx] - c, axis=-1) / r) ** 2) * s_)
for sd in "lr":
    for f in ("index", "middle", "ring", "pinky"):
        for seg in ("01", "02"):
            c = bones[f"{f}_{seg}_{sd}"][0]; yy, xx = region(c, 0.012)
            M["knuckle"][yy, xx] = np.maximum(M["knuckle"][yy, xx], np.exp(-(np.linalg.norm(POS[yy, xx] - c, axis=-1) / 0.007) ** 2) * 0.2)
def freckles(sel, count, rpx, op):
    yy, xx = np.where(sel)
    if not len(yy): return
    for k in rng.choice(len(yy), size=min(count, len(yy)), replace=False):
        y, x = yy[k], xx[k]; r = rng.uniform(*rpx); R = int(r * 3) + 1
        y0, y1, x0, x1 = max(0, y - R), min(RES, y + R + 1), max(0, x - R), min(RES, x + R + 1)
        gy, gx = np.mgrid[y0:y1, x0:x1]
        M["freckle"][y0:y1, x0:x1] = np.maximum(M["freckle"][y0:y1, x0:x1], np.exp(-((gy - y) ** 2 + (gx - x) ** 2) / (r * r)) * rng.uniform(*op))
chest_top = max(NIP[0][2], NIP[1][2]) + 0.05
freckles(COV & (NRM[..., 1] < -0.3) & (POS[..., 2] > chest_top) & (POS[..., 2] < EL[2] - 0.17), 380, (0.8, 1.8), (0.25, 0.9))
freckles(COV & (NRM[..., 2] > 0.45) & (POS[..., 2] > chest_top + 0.04) & (POS[..., 2] < EL[2] - 0.15), 340, (0.8, 1.8), (0.25, 0.9))
freckles(COV & (np.linalg.norm(POS - (0.5 * (EL + ER) + np.array([0, -0.025, -0.02])), axis=-1) < 0.032) & (NRM[..., 1] < -0.2), 120, (1.6, 3.0), (0.2, 0.6))
M["freckle"] *= COV
mole = EL + np.array([0.021, -0.016, -0.036])            # beauty mark high on her left cheekbone
loc, *_ = bvh_body.find_nearest(tuple(mole)); yy, xx = region(np.array(loc), 0.004)
M["mole"][yy, xx] = ss(0.0012, 0.0006, np.linalg.norm(POS[yy, xx] - np.array(loc), axis=-1))
face = ss(0.3, 0.7, MH) * COV
pores = face * ss(0.4, 0.1, HP[..., 0])
yy, xx = np.where(M["lip"] > 0.01); H[yy, xx] += (HP[yy, xx, 1] - 0.5) * 0.35 * M["lip"][yy, xx]
ROUGH = ROUGH * (1 - M["lip"]) + 0.3 * M["lip"]
ROUGH = ROUGH * (1 - M["nail"]) + 0.12 * M["nail"]
ROUGH = np.minimum(ROUGH, 0.52 - 0.12 * M["shadow"])
log("all paint masks ready")

# ================================================================== tones
def to_lin(c): return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)
def to_srgb(c): c = np.clip(c, 0, 1); return np.where(c <= 0.0031308, c * 12.92, 1.055 * c ** (1 / 2.4) - 0.055)
def load_src(k): return to_lin(np.flipud(np.asarray(Image.open(SRC[k]).convert("RGB").resize((RES, RES), Image.LANCZOS), np.float32) / 255.0))
chest = COV & (NRM[..., 1] < -0.5) & (np.abs(POS[..., 0]) < 0.06) & (POS[..., 2] > NIP[0][2] + 0.04) & (POS[..., 2] < NIP[0][2] + 0.11)
TONES = {
    #          source blend (cau, afr)  chest target sRGB         lip colour              eyeshadow mult        blush  flush mult            areola mult         freckles
    "honey":   dict(vulva=dict(maj=(0.86, 0.74, 0.70), anus=(0.70, 0.52, 0.50), mino=(0.72, 0.52, 0.50), muc=(0.80, 0.38, 0.40), clit=(0.85, 0.45, 0.48), deep=(0.22, 0.04, 0.05)),
                     mix=(1.0, 0.0), chest=(206, 152, 114), lip=(0.50, 0.08, 0.13), shadow=(0.76, 0.60, 0.62), blush=0.26, flush=(1.0, 0.80, 0.78), areola=(0.78, 0.60, 0.56), freck=0.30),
    "caramel": dict(vulva=dict(maj=(0.80, 0.68, 0.64), anus=(0.62, 0.46, 0.44), mino=(0.62, 0.45, 0.43), muc=(0.70, 0.30, 0.33), clit=(0.74, 0.36, 0.38), deep=(0.18, 0.03, 0.04)),
                     mix=(0.45, 0.55), chest=(170, 112, 78), lip=(0.38, 0.065, 0.10), shadow=(0.72, 0.57, 0.55), blush=0.18, flush=(0.93, 0.83, 0.81), areola=(0.66, 0.52, 0.48), freck=0.12),
    "deep":    dict(vulva=dict(maj=(0.78, 0.66, 0.62), anus=(0.60, 0.46, 0.44), mino=(0.58, 0.44, 0.42), muc=(0.55, 0.20, 0.23), clit=(0.58, 0.25, 0.28), deep=(0.12, 0.02, 0.03)),
                     mix=(0.0, 1.0), chest=(112, 68, 47), lip=(0.25, 0.045, 0.07), shadow=(0.78, 0.63, 0.56), blush=0.12, flush=(0.86, 0.82, 0.80), areola=(0.62, 0.50, 0.46), freck=0.0),
}
SRCL = {k: load_src(k) for k in SRC}
def lerp3(a, b, t): return a + (b - a) * t[..., None]
for tone, T in TONES.items():
    base = SRCL["cau"] * T["mix"][0] + SRCL["afr"] * T["mix"][1]
    gain = to_lin(np.array(T["chest"], np.float32) / 255) / base[chest].mean(0)
    C = base * gain
    mott = HP[..., 2] - 0.5
    C *= (1 + mott[..., None] * np.array([0.06, 0.0, -0.03]))
    C *= (0.82 + 0.18 * AO)[..., None]
    C = lerp3(C, C * np.array(T["flush"]), M["flush"]); C = lerp3(C, C * np.array(T["flush"]), M["knuckle"])
    C = lerp3(C, C * np.array([0.78, 0.62, 0.52]), M["freckle"] * T["freck"])
    C = lerp3(C, C * np.array([0.42, 0.30, 0.26]), M["mole"] * 0.85)
    C = lerp3(C, C * np.array(T["shadow"]) ** 2.2, M["shadow"] * 0.6)
    C = lerp3(C, C * np.array([0.40, 0.30, 0.26]) ** 2.2, M["browfill"] * 0.55)
    C = lerp3(C, np.full(3, 0.004), M["liner"] * 0.92)
    C = lerp3(C, C * np.array([0.55, 0.42, 0.42]) ** 2.2, M["smudge"] * 0.5)
    C = lerp3(C, C * np.array([1.0, 0.70, 0.72]) ** 2.2, M["blush"] * T["blush"])
    lipc = to_lin(np.array(T["lip"])) * np.ones_like(C)
    lipc = lerp3(lipc, to_lin(np.array([0.60, 0.22, 0.25])) * np.array([1.0, 0.92, 0.92]) * (0.5 if tone == "deep" else 1.0), M["wet"])
    detail = (C.mean(-1, keepdims=True) / np.maximum(1e-4, C[M["lip"] > 0.5].mean())) ** 0.5
    C = lerp3(C, lipc * np.clip(detail, 0.6, 1.4), M["lip"] * 0.9)
    C = lerp3(C, to_lin(np.array([0.36, 0.025, 0.055])) * (0.85 + 0.3 * HP[..., 2:3]), M["nail"] * 0.96)
    C = lerp3(C, C * np.array(T["areola"]) ** 2.2, M["areola"] * 0.85)
    C = lerp3(C, C * np.array(T["areola"]) ** 3.0, M["nipple"] * 0.6)
    Vp = T["vulva"]
    C = lerp3(C, C * np.array(Vp["maj"]) ** 2.2, M["v_maj"] * 0.8)                 # outer labia + perineum pigment
    C = lerp3(C, C * np.array(Vp["anus"]) ** 2.2, M["v_anus"] * 0.85)
    C = lerp3(C, C * np.array(Vp["mino"]) ** 2.2, M["v_min"] * 0.8)                # inner labia, darker edges
    C = lerp3(C, to_lin(np.array(Vp["muc"])) * (0.85 + 0.3 * HP[..., 2:3]), M["v_muc"] * 0.92)
    C = lerp3(C, to_lin(np.array(Vp["clit"])), M["v_clit"] * 0.5 * (1 - M["v_cav"]))
    C = lerp3(C, to_lin(np.array(Vp["deep"])), np.clip(M["v_cav"], 0, 1) ** 0.8)       # canals darken with depth
    C = lerp3(C, to_lin(np.array([0.07, 0.04, 0.03])), M["v_pub"])                    # trimmed landing strip
    out = np.flipud(np.clip(to_srgb(C) * 255 + 0.5, 0, 255).astype(np.uint8))
    Image.fromarray(out).save(f"{TEX}/vivian_skin_{tone}.jpg", quality=92)
    Image.fromarray(out).resize((768, 768), Image.LANCZOS).save(f"{TEX}/preview_skin_{tone}.jpg", quality=85)
    log(f"skin tone '{tone}' painted (chest {T['chest']})")
del SRCL

# shared normal + ORM
h = H - 0.35 * pores + (HP[..., 1] - 0.5) * (0.55 + 0.25 * face) + (HP[..., 2] - 0.5) * 0.25
ROUGH = ROUGH + pores * 0.05 + (HP[..., 1] - 0.5) * 0.06
ROUGH[COV & (np.linalg.norm(POS - (0.5 * (EL + ER) + np.array([0, -0.02, 0.02])), axis=-1) < 0.05)] -= 0.06
dx = np.zeros_like(h); dy = np.zeros_like(h)
dx[:, 1:-1] = (h[:, 2:] - h[:, :-2]) * 0.5; dy[1:-1, :] = (h[2:, :] - h[:-2, :]) * 0.5
n = np.stack([-dx * 0.9, -dy * 0.9, np.ones_like(h)], -1); n /= np.linalg.norm(n, axis=-1, keepdims=True)
ORM = np.stack([np.clip(0.25 + 0.75 * AO, 0, 1), np.clip(ROUGH, 0.1, 0.9), np.zeros_like(ROUGH)], -1)
def save(a, path, q=None):
    im = Image.fromarray(np.flipud(np.clip(a * 255 + 0.5, 0, 255).astype(np.uint8))); im.save(path, quality=q) if q else im.save(path)
save(n * 0.5 + 0.5, TEX + "/vivian_skin_normal.jpg", 93); save(ORM, TEX + "/vivian_skin_orm.jpg", 92)
log(f"normal + ORM written ({time.time()-t0:.0f}s)")

# ================================================================== materials
def timg(nt, path, cs):
    n = nt.nodes.new("ShaderNodeTexImage"); n.image = bpy.data.images.load(path); n.image.colorspace_settings.name = cs; return n
mat = bpy.data.materials.new("vivian_skin"); mat.use_nodes = True; nt = mat.node_tree; nt.nodes.clear()
out = nt.nodes.new("ShaderNodeOutputMaterial"); bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled"); nt.links.new(bsdf.outputs[0], out.inputs[0])
bc = timg(nt, f"{TEX}/vivian_skin_{DEFAULT_TONE}.jpg", "sRGB"); bc.name = "SKIN_BASE"; nt.links.new(bc.outputs[0], bsdf.inputs["Base Color"])
nmi = timg(nt, TEX + "/vivian_skin_normal.jpg", "Non-Color"); nmap = nt.nodes.new("ShaderNodeNormalMap")
nt.links.new(nmi.outputs[0], nmap.inputs["Color"]); nt.links.new(nmap.outputs[0], bsdf.inputs["Normal"])
orm = timg(nt, TEX + "/vivian_skin_orm.jpg", "Non-Color"); sep = nt.nodes.new("ShaderNodeSeparateColor")
nt.links.new(orm.outputs[0], sep.inputs[0]); nt.links.new(sep.outputs[1], bsdf.inputs["Roughness"])
g = bpy.data.node_groups.get("glTF Material Output") or bpy.data.node_groups.new("glTF Material Output", "ShaderNodeTree")
if not g.interface.items_tree: g.interface.new_socket("Occlusion", in_out="INPUT", socket_type="NodeSocketFloat")
gn = nt.nodes.new("ShaderNodeGroup"); gn.node_tree = g; nt.links.new(sep.outputs[0], gn.inputs["Occlusion"])
bsdf.inputs["Subsurface Weight"].default_value = 0.2
bsdf.inputs["Subsurface Radius"].default_value = (1.0, 0.38, 0.2); bsdf.inputs["Subsurface Scale"].default_value = 0.01
for s in body.material_slots: s.material = mat
def tex_mat(name, img_path, rough, tint=None, alpha=False):
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; nt.nodes.clear()
    o_ = nt.nodes.new("ShaderNodeOutputMaterial"); bs = nt.nodes.new("ShaderNodeBsdfPrincipled"); nt.links.new(bs.outputs[0], o_.inputs[0])
    ti = timg(nt, img_path, "sRGB")
    if tint:
        mx = nt.nodes.new("ShaderNodeMix"); mx.data_type = "RGBA"; mx.blend_type = "MULTIPLY"; mx.inputs["Factor"].default_value = 1
        mx.inputs[7].default_value = (*tint, 1); nt.links.new(ti.outputs[0], mx.inputs[6]); nt.links.new(mx.outputs[2], bs.inputs["Base Color"])
    else:
        nt.links.new(ti.outputs[0], bs.inputs["Base Color"])
    if alpha: nt.links.new(ti.outputs[1], bs.inputs["Alpha"]); m.blend_method = "HASHED"
    bs.inputs["Roughness"].default_value = rough
    return m
EYES = "/root/agb/mh_assets/eyes/materials/"
ge = np.asarray(Image.open(EYES + "green_eye.png").convert("RGBA"), np.float32); be = np.asarray(Image.open(EYES + "brownlight_eye.png").convert("RGBA"), np.float32)
hz = ge.copy(); hz[..., :3] = np.clip((0.28 * ge[..., :3] + 0.72 * be[..., :3]) * np.array([0.80, 0.70, 0.58]), 0, 255)
Image.fromarray(hz.astype(np.uint8), "RGBA").save(TEX + "/vivian_eye.png")
br = Image.open("/root/agb/mh_assets/eyebrows/eyebrow010/eyebrow010.png").convert("RGBA"); ba = np.asarray(br, np.float32)
nz = np.asarray(Image.effect_noise(br.size, 64).convert("L"), np.float32) / 255
rgb = np.array([40, 26, 19], np.float32)[None, None] * (0.75 + 0.5 * nz[..., None])
Image.fromarray(np.dstack([np.clip(rgb, 0, 255), ba[..., 3:]]).astype(np.uint8), "RGBA").save(TEX + "/vivian_brow.png")
def first_img(o):
    for nd in o.material_slots[0].material.node_tree.nodes:
        if nd.type == "TEX_IMAGE" and nd.image: return bpy.path.abspath(nd.image.filepath)
PARTS["eyes"].material_slots[0].material = tex_mat("vivian_eye", TEX + "/vivian_eye.png", 0.04, alpha=True)
PARTS["eyebrows"].material_slots[0].material = tex_mat("vivian_brow", TEX + "/vivian_brow.png", 0.6, alpha=True)
PARTS["eyelashes"].material_slots[0].material = tex_mat("vivian_lash", first_img(PARTS["eyelashes"]), 0.5, alpha=True)
PARTS["teeth"].material_slots[0].material = tex_mat("vivian_teeth", first_img(PARTS["teeth"]), 0.28, tint=(0.86, 0.83, 0.76))
PARTS["tongue"].material_slots[0].material = tex_mat("vivian_tongue", first_img(PARTS["tongue"]), 0.35, tint=(0.95, 0.82, 0.82))
json.dump({"EL": EL.tolist(), "ER": ER.tolist(), "MOUTH": MOUTH.tolist(), "NIP": [x.tolist() for x in NIP],
           "bones": {k: [v[0].tolist(), v[1].tolist()] for k, v in bones.items()}}, open(OUT + "/landmarks.json", "w"))
bpy.ops.wm.save_as_mainfile(filepath=OUT + "/vivian_s4.blend")
log(f"stage4 saved ({time.time()-t0:.0f}s)")
