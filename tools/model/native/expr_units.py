# More expressive faces, step 1 of 2 (MPFB): expression morphs for a base character, built from MakeHuman's
# expression units exactly like build5's Vivian v3 set, carried to the eyebrows / lashes / teeth / tongue through
# MakeHuman's own MHCLO fitting weights.
#   blender -b --python tools/model/native/expr_units.py -- <base: marcus|vivian> <out.npz> [name,name,...]
# The base is rebuilt from tools/model/identity/recipes.json (the same recipe the identity packs use), so the result
# lines up with the shipped GLB through expr_apply.py's checked vertex correspondence.
import bpy, sys, os, json, glob, time
import numpy as np
from bl_ext.user_default.mpfb.services.humanservice import HumanService
from bl_ext.user_default.mpfb.services.assetservice import AssetService
from bl_ext.user_default.mpfb.services.targetservice import TargetService
from bl_ext.user_default.mpfb.entities.clothes.mhclo import Mhclo

argv = sys.argv[sys.argv.index("--") + 1:]
BASE_ID, OUTF = argv[0], argv[1]
ONLY = set(argv[2].split(",")) if len(argv) > 2 and argv[2] else None
HERE = os.path.dirname(os.path.abspath(__file__))
R = json.load(open(os.path.join(HERE, "..", "identity", "recipes.json")))
BASE = R["bases"][BASE_ID]
MPFB = os.path.expanduser("~/.config/blender/4.2/extensions/user_default/mpfb/data/targets")
known = {os.path.basename(p)[:-10] for p in glob.glob(MPFB + "/**/*.target.gz", recursive=True)}
t0 = time.time()
def log(*a): print("[expr_units]", *a, flush=True)

bpy.ops.wm.read_factory_settings(use_empty=True)
info = HumanService._create_default_human_info_dict()
info["phenotype"] = BASE["phenotype"]
info["targets"] = [{"target": t, "value": v} for t, v in BASE["targets"].items() if t in known and v > 0]
st = HumanService.get_default_deserialization_settings(); st["subdiv_levels"] = 0
body = HumanService.deserialize_from_dict(info, st)
TargetService.bake_targets(body)
def co(o):
    a = np.empty(len(o.data.vertices) * 3, np.float32); o.data.vertices.foreach_get("co", a); return a.reshape(-1, 3).astype(np.float64)
B0 = co(body)
log(f"base {BASE_ID}: {len(B0)} verts")

# expression units as deltas on this base
body.shape_key_add(name="Basis", from_mix=False)
unit = {}
race = BASE["phenotype"].get("race", {})
units_dir = MPFB + "/expression/units/" + ("african" if race.get("african", 0) > 0.6 else ("asian" if race.get("asian", 0) > 0.6 else "caucasian"))
for f in sorted(glob.glob(units_dir + "/*.target.gz")):
    name = os.path.basename(f)[:-10]
    TargetService.load_target(body, f, weight=0.0, name="u_" + name)
    kb = body.data.shape_keys.key_blocks["u_" + name]
    a = np.empty(len(kb.data) * 3, np.float32); kb.data.foreach_get("co", a)
    unit[name] = a.reshape(-1, 3).astype(np.float64) - B0
log(f"{len(unit)} expression units from {os.path.basename(units_dir)}")
U = lambda n, w: unit[n] * w
def ss(e0, e1, x):
    t = np.clip((np.asarray(x, np.float64) - e0) / (e1 - e0), 0, 1); return t * t * (3 - 2 * t)
dn_open = np.linalg.norm(unit["mouth-open"], axis=1)
m_low = np.clip(dn_open / max(float(dn_open.max()), 1e-9) * 2.5, 0, 1)[:, None]   # lower lip + jaw
sL = ss(-0.006, 0.006, B0[:, 0])[:, None]                                         # the character's left half (+X)
both = lambda a, w: U(f"{a.replace('*', 'left')}", w) + U(f"{a.replace('*', 'right')}", w)

# a man's set: warmth, humour, interest, concern, surprise, desire; lids that follow the gaze. Subtle weights: the
# engine layers them (facelife, acting, the night) and a sum of two must still read as one face.
EXPR = {
    "smile":     U("mouth-corner-puller", 0.7) + U("mouth-elevation", 0.15) + both("eye-*-slit", 0.25),
    "laugh":     U("mouth-corner-puller", 0.85) + U("mouth-open", 0.32) + U("mouth-parling", 0.3) + both("eye-*-slit", 0.45)
                 + both("eyebrows-*-inner-up", 0.12) + both("nose-*-elevation", 0.15),
    "smirk":     (U("mouth-corner-puller", 0.6) + U("mouth-upward-retraction", 0.2)) * sL + U("eye-left-slit", 0.15)
                 + U("eyebrows-right-up", 0.25),
    "brow_up":   both("eyebrows-*-up", 0.8) + both("eye-*-opened-up", 0.25),
    "frown":     both("eyebrows-*-down", 0.7) + U("mouth-depression", 0.25) + U("mouth-compression", 0.2) + both("eye-*-slit", 0.15),
    "gasp":      U("mouth-open", 0.55) + U("mouth-parling", 0.3) + both("eye-*-opened-up", 0.6) + both("eyebrows-*-up", 0.7)
                 + both("nose-*-dilatation", 0.4) + U("neck-platysma", 0.1),
    "bite_lip":  U("mouth-eversion", -0.6) * m_low + U("mouth-elevation", 0.15) + U("mouth-corner-puller", 0.15) + U("mouth-open", 0.05)
                 + both("eye-*-slit", 0.2),
    "wink":      U("eye-left-closure", 1.0) + U("mouth-corner-puller", 0.4) * sL + U("nose-left-elevation", 0.2) + U("eyebrows-left-down", 0.2),
    "tender":    U("mouth-corner-puller", 0.3) + both("eyebrows-*-inner-up", 0.25) + both("eye-*-slit", 0.2),
    "lids_down": both("eye-*-closure", 0.35),
    "lids_up":   both("eye-*-opened-up", 0.6),
}
if ONLY: EXPR = {k: v for k, v in EXPR.items() if k in ONLY}

# proxies through MHCLO weights (eyes stay put: no unit moves the eyeballs)
out = {"B0": B0.astype(np.float32)}
PROX = {}
for sub, fn in BASE["proxies"].items():
    p = AssetService.find_asset_absolute_path(fn, asset_subdir=sub)
    m = Mhclo(); m.load(p)
    n = max(m.verts.keys()) + 1
    idx = np.zeros((n, 3), np.int64); w = np.zeros((n, 3)); ok = np.zeros(n, bool)
    for vi, mp in m.verts.items(): idx[vi] = mp["verts"]; w[vi] = mp["weights"]; ok[vi] = True
    PROX[sub] = (idx, w, ok)
for name, d in EXPR.items():
    out["D_" + name] = d.astype(np.float32)
    for sub, (idx, w, ok) in PROX.items():
        out[f"P_{sub}_{name}"] = ((d[idx] * w[..., None]).sum(1) * ok[:, None]).astype(np.float32)
    log(f"  {name}: max {np.abs(d).max()*1000:.1f} mm, {int((np.abs(d).max(1) > 1e-5).sum())} verts move")
out["names"] = np.array(list(EXPR))
np.savez_compressed(OUTF, **out)
log(f"saved {OUTF} ({time.time()-t0:.0f}s)")
