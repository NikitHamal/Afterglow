# A new face on a proven rig, step 1 of 2 (MPFB): the identity delta of a model = MakeHuman(base recipe + the model's
# head phenotype + face targets) - MakeHuman(base recipe), kept to the head and neck, carried to the eyes / brows /
# lashes / teeth / tongue through MakeHuman's own MHCLO fitting weights (exactly like build5's body variants).
#   blender -b --python tools/model/identity/identity.py -- <model id> <out.npz>
# The eyeballs are pinned: the delta around each eye is shifted so the eyeball centre stays where the base's eye bone
# is (gaze rotates about the right point). The body below the neck stays the base's (clips, outfits, contacts fit).
import bpy, sys, os, json, glob, time
import numpy as np
from bl_ext.user_default.mpfb.services.humanservice import HumanService
from bl_ext.user_default.mpfb.services.assetservice import AssetService
from bl_ext.user_default.mpfb.services.targetservice import TargetService
from bl_ext.user_default.mpfb.entities.clothes.mhclo import Mhclo

argv = sys.argv[sys.argv.index("--") + 1:]
MID, OUTF = argv[0], argv[1]
HERE = os.path.dirname(os.path.abspath(__file__))
R = json.load(open(os.path.join(HERE, "recipes.json")))
M = R["models"][MID]; BASE = R["bases"][M["base"]]
def _mpfb_targets():
    # the extension lives under the Blender version that installed it; 4.2 was the original build, 4.3 is what
    # Debian ships, and a source checkout is the fallback for a box that has neither
    for v in ("4.3", "4.2"):
        p = os.path.expanduser(f"~/.config/blender/{v}/extensions/user_default/mpfb/data/targets")
        if os.path.isdir(p):
            return p
    return "/root/mpfb2/src/mpfb/data/targets"

MPFB = _mpfb_targets()
known = {os.path.basename(p)[:-10] for p in glob.glob(MPFB + "/**/*.target.gz", recursive=True)}
t0 = time.time()
def log(*a): print("[identity]", *a, flush=True)
assert M["phenotype"].get("age", 0.5) >= 0.5, "adults only: MakeHuman age >= 0.5 (25 y)"

def build(pheno, targets, rig=False):
    info = HumanService._create_default_human_info_dict()
    info["phenotype"] = pheno
    info["targets"] = [{"target": t, "value": v} for t, v in targets.items() if t in known and v > 0]
    s = HumanService.get_default_deserialization_settings(); s["subdiv_levels"] = 0
    b = HumanService.deserialize_from_dict(info, s)
    if rig: HumanService.add_builtin_rig(b, "game_engine_with_breast" if BASE["phenotype"]["gender"] < 0.5 else "game_engine", import_weights=True)
    TargetService.bake_targets(b)
    return b
def co(o):
    a = np.empty(len(o.data.vertices) * 3, np.float32); o.data.vertices.foreach_get("co", a); return a.reshape(-1, 3).astype(np.float64)
def vg(o, name):
    w = np.zeros(len(o.data.vertices), np.float64)
    if name not in o.vertex_groups: return w
    gi = o.vertex_groups[name].index
    for v in o.data.vertices:
        for g in v.groups:
            if g.group == gi: w[v.index] = g.weight
    return w
def drop(o):
    for c in list(o.children): bpy.data.objects.remove(c, do_unlink=True)
    bpy.data.objects.remove(o, do_unlink=True)

bpy.ops.wm.read_factory_settings(use_empty=True)
unknown = [t for t in list(M["targets"]) + list(BASE["targets"]) if t not in known]
if unknown: log("WARNING unknown targets (ignored):", unknown)
b0 = build(BASE["phenotype"], BASE["targets"], rig=True)
B0 = co(b0)
head = vg(b0, "head"); neck = vg(b0, "neck_01")
log(f"base {M['base']}: {len(B0)} verts, head group {int((head > 0.5).sum())} verts")
ph = json.loads(json.dumps(BASE["phenotype"])); ph.update({k: v for k, v in M["phenotype"].items() if k != "race"})
if "race" in M["phenotype"]: ph["race"] = M["phenotype"]["race"]
tg = dict(BASE["targets"])
for k, v in M["targets"].items(): tg[k] = max(0.0, tg.get(k, 0.0) + v)
b1 = build(ph, tg)
B1 = co(b1); drop(b1)
D = B1 - B0
# a younger / other-heritage MakeHuman is also taller or shorter: the head rides along. Anchor the identity to the
# skull: remove the cranium's mean drift (top and back of the head, above the eyes), keep shape differences only.
eyez = B0[head > 0.9][:, 2]
cran = (head > 0.9) & (B0[:, 2] > np.percentile(eyez, 55))
t = D[cran].mean(0)
log(f"head drift removed: {np.round(t*1000, 1)} mm (cranium {int(cran.sum())} verts)")
mask = np.clip(head + 0.8 * neck, 0, 1)
mask = mask * mask * (3 - 2 * mask)
D = (D - t[None]) * mask[:, None]
log(f"head delta: max {np.abs(D).max()*1000:.1f} mm, mean {np.linalg.norm(D, axis=1)[mask > 0.5].mean()*1000:.2f} mm")

# proxies through MHCLO weights
PROX = {}
for sub, fn in BASE["proxies"].items():
    p = AssetService.find_asset_absolute_path(fn, asset_subdir=sub)
    if not p:
        log(f"proxy {sub} not installed, carried from the head field instead")
        continue
    m = Mhclo(); m.load(p)
    n = max(m.verts.keys()) + 1
    idx = np.zeros((n, 3), np.int64); w = np.zeros((n, 3)); ok = np.zeros(n, bool)
    for vi, mp in m.verts.items(): idx[vi] = mp["verts"]; w[vi] = mp["weights"]; ok[vi] = True
    PROX[sub] = (idx, w, ok)
def carry(Dv, sub):
    idx, w, ok = PROX[sub]
    return (Dv[idx] * w[..., None]).sum(1) * ok[:, None]
# pin the eyeballs: shift the delta around each eye by that eye's centre motion (full within 16 mm, none past 34 mm);
# a few passes, because the eye's own MHCLO anchors sit inside that falloff
idx_e, w_e, _ = PROX["eyes"]
eyes0 = (B0[idx_e] * w_e[..., None]).sum(1)
for it in range(4):
    De = carry(D, "eyes")
    for s in (1, -1):
        sel = np.sign(eyes0[:, 0]) == s
        c0 = eyes0[sel].mean(0); dc = De[sel].mean(0)
        dist = np.linalg.norm(B0 - c0, axis=1)
        f = np.clip((0.034 - dist) / 0.018, 0, 1); f = f * f * (3 - 2 * f)
        D -= f[:, None] * dc[None]
        if it == 0: log(f"eye {'L' if s > 0 else 'R'}: centre moved {np.linalg.norm(dc)*1000:.2f} mm -> pinned")
out = {"B0": B0.astype(np.float32), "D": D.astype(np.float32), "mask": mask.astype(np.float32)}
for sub in PROX:
    out["P_" + sub] = carry(D, sub).astype(np.float32)
    out["P0_" + sub] = carry(B0, sub).astype(np.float32)
for s in (1, -1):
    sel = np.sign(eyes0[:, 0]) == s
    log(f"eye {'L' if s > 0 else 'R'} after pin: {np.linalg.norm(out['P_eyes'][sel].mean(0))*1000:.3f} mm")
np.savez_compressed(OUTF, **out)
log(f"saved {OUTF} ({time.time()-t0:.0f}s)")
