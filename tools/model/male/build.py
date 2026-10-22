# Marcus (adult male, ~40, athletic) — stage A: body + variants as shape keys, expressions, anatomically built genitals.
#   blender -b --python male_build.py        (runs in /root/agb/male)
import bpy, bmesh, os, math, time, glob, json, shlex
import numpy as np
from mathutils import Vector
from bl_ext.user_default.mpfb.services.humanservice import HumanService
from bl_ext.user_default.mpfb.services.assetservice import AssetService
from bl_ext.user_default.mpfb.services.targetservice import TargetService
from bl_ext.user_default.mpfb.entities.clothes.mhclo import Mhclo

M = "/root/agb/male"; OUT = M + "/out"
MPFB = "/home/marimo/.config/blender/4.2/extensions/user_default/mpfb/data/targets"
def prog(*a): os.system("python3 /root/agb/progress.py " + " ".join(shlex.quote(str(x)) for x in a))
def log(*a): print("[male]", *a, flush=True); prog("log", "[male] " + " ".join(str(x) for x in a))
t0 = time.time()
known = {os.path.basename(p)[:-10] for p in glob.glob(MPFB + "/**/*.target.gz", recursive=True)}

BASE = {"gender": 1.0, "age": 0.62, "muscle": 0.68, "weight": 0.55, "proportions": 0.85, "height": 0.62,
        "cupsize": 0.5, "firmness": 0.5, "race": {"caucasian": 0.6, "african": 0.25, "asian": 0.15}}
FACE = {"chin-prominent-incr": 0.22, "chin-width-incr": 0.12, "head-square": 0.3, "l-cheek-bones-incr": 0.25, "r-cheek-bones-incr": 0.25,
        "l-cheek-volume-decr": 0.2, "r-cheek-volume-decr": 0.2, "eyebrows-trans-down": 0.18, "nose-hump-decr": 0.15,
        "mouth-lowerlip-volume-incr": 0.1, "measure-neck-circ-incr": 0.3,
        "torso-vshape-incr": 0.45, "measure-shoulder-dist-incr": 0.18, "measure-waist-circ-decr": 0.18,
        "torso-muscle-pectoral-incr": 0.3, "torso-muscle-dorsi-incr": 0.3, "stomach-pregnant-decr": 0.25, "buttocks-volume-incr": 0.12}
def build_human(pheno, extra=None, parts=False):
    info = HumanService._create_default_human_info_dict()
    info["phenotype"] = pheno
    tg = dict(FACE); tg.update(extra or {})
    info["targets"] = [{"target": t, "value": v} for t, v in tg.items() if t in known]
    info["skin_mhmat"] = "middleage_caucasian_male/middleage_caucasian_male.mhmat"
    info["skin_material_type"] = "GAMEENGINE"
    s = HumanService.get_default_deserialization_settings(); s["subdiv_levels"] = 0
    b = HumanService.deserialize_from_dict(info, s)
    TargetService.bake_targets(b)
    return b
def co(o):
    a = np.empty(len(o.data.vertices) * 3, np.float32); o.data.vertices.foreach_get("co", a); return a.reshape(-1, 3)
def drop(o):
    for c in list(o.children): bpy.data.objects.remove(c, do_unlink=True)
    bpy.data.objects.remove(o, do_unlink=True)

bpy.ops.wm.read_factory_settings(use_empty=True)
# ---- shape variants (same topology) captured first, then discarded
VARS = {}
for name, ph, ex in [("body_muscle", dict(BASE, muscle=1.0), None), ("body_weight", dict(BASE, weight=0.92), None),
                     ("body_belly", BASE, {"stomach-pregnant-incr": 0.55, "stomach-pregnant-decr": 0.0})]:
    v = build_human(ph, ex); VARS[name] = co(v); drop(v)
log(f"variants captured: {list(VARS)}")

body = build_human(BASE)
rig = HumanService.add_builtin_rig(body, "game_engine_with_breast", import_weights=True)
PARTS = {}
for sub, fn in [("eyes", "high-poly/high-poly.mhclo"), ("eyebrows", "eyebrow001/eyebrow001.mhclo"),
                ("eyelashes", "eyelashes01/eyelashes01.mhclo"), ("teeth", "teeth_base/teeth_base.mhclo"),
                ("tongue", "tongue01/tongue01.mhclo")]:
    p = AssetService.find_asset_absolute_path(fn, asset_subdir=sub)
    before = set(bpy.data.objects)
    HumanService.add_mhclo_asset(p, body, asset_type=sub, subdiv_levels=0, material_type="MAKESKIN")
    PARTS[sub] = [o for o in bpy.data.objects if o not in before and o.type == "MESH"][0]
    PARTS[sub]["mhclo_path"] = p
for o in PARTS.values():
    if o.data.shape_keys:
        o.shape_key_add(name="__mix", from_mix=True)
        for k in list(o.data.shape_keys.key_blocks): o.shape_key_remove(k)
B0 = co(body)
log(f"base male built: {len(B0)} verts, height {B0[:, 2].max():.3f} m")

# ---- expressions from MakeHuman expression units
body.shape_key_add(name="Basis", from_mix=False)
unit = {}
for f in sorted(glob.glob(MPFB + "/expression/units/caucasian/*.target.gz")):
    n = os.path.basename(f)[:-10]
    TargetService.load_target(body, f, weight=0.0, name="u_" + n)
    kb = body.data.shape_keys.key_blocks["u_" + n]; a = np.empty(len(kb.data) * 3, np.float32); kb.data.foreach_get("co", a)
    unit[n] = a.reshape(-1, 3) - B0
for kb in list(body.data.shape_keys.key_blocks):
    if kb.name.startswith("u_"): body.shape_key_remove(kb)
U = lambda n, w: unit[n] * w
EXPR = {
    "blink":    U("eye-left-closure", 1) + U("eye-right-closure", 1),
    "effort":   U("eyebrows-left-down", 0.7) + U("eyebrows-right-down", 0.7) + U("eye-left-slit", 0.45) + U("eye-right-slit", 0.45)
                + U("mouth-compression", 0.5) + U("nose-left-elevation", 0.3) + U("nose-right-elevation", 0.3),
    "pleasure": U("eyebrows-left-inner-up", 0.7) + U("eyebrows-right-inner-up", 0.7) + U("eye-left-slit", 0.55) + U("eye-right-slit", 0.55)
                + U("mouth-parling", 0.45) + U("mouth-open", 0.15),
    "moan":     U("mouth-open", 0.5) + U("mouth-parling", 0.3) + U("eyebrows-left-inner-up", 0.4) + U("eyebrows-right-inner-up", 0.4)
                + U("eye-left-slit", 0.4) + U("eye-right-slit", 0.4),
    "kiss":     U("mouth-pursing", 0.75) + U("mouth-protusion", 0.45) + U("eye-left-closure", 0.8) + U("eye-right-closure", 0.8),
}
for n, d in EXPR.items():
    kb = body.shape_key_add(name=n, from_mix=False); kb.data.foreach_set("co", (B0 + d).ravel())
for n, X in VARS.items():
    kb = body.shape_key_add(name=n, from_mix=False); kb.data.foreach_set("co", X.ravel())
for sub, o in PARTS.items():                      # carry expressions + body variants onto lashes/brows/teeth/tongue/eyes
    m = Mhclo(); m.load(o["mhclo_path"])
    nv = len(o.data.vertices); idx = np.zeros((nv, 3), np.int64); w = np.zeros((nv, 3), np.float32); ok = np.zeros(nv, bool)
    for vi, mp in m.verts.items():
        if vi < nv: idx[vi] = mp["verts"]; w[vi] = mp["weights"]; ok[vi] = True
    P0 = co(o); o.shape_key_add(name="Basis", from_mix=False)
    for n, d in list(EXPR.items()) + [(k, X - B0) for k, X in VARS.items()]:
        off = (d[idx] * w[..., None]).sum(1) * ok[:, None]
        if np.abs(off).max() < 1e-4: continue
        kb = o.shape_key_add(name=n, from_mix=False); kb.data.foreach_set("co", (P0 + off).ravel())
log("expressions + variant keys on body and face parts")

# ---- drop helper geometry (shape layers survive)
gb = body.vertex_groups["body"].index
bm = bmesh.new(); bm.from_mesh(body.data); dl = bm.verts.layers.deform.active
bmesh.ops.delete(bm, geom=[v for v in bm.verts if v[dl].get(gb, 0.0) < 0.5], context="VERTS")
bm.to_mesh(body.data); bm.free(); body.data.update()
for m_ in list(body.modifiers):
    if m_.type == "MASK": body.modifiers.remove(m_)
log(f"helpers removed: {len(body.data.vertices)} verts")
bpy.ops.wm.save_as_mainfile(filepath=OUT + "/marcus_a.blend")
log(f"stage A saved ({time.time()-t0:.0f}s)")
