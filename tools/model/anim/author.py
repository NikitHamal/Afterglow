# Author + bake + verify all clips with both characters in one world.
#   blender -b --python author.py -- [clip,clip,...] [--render] [--norender]
# -> /root/agb/native/anim.blend (actions VIV|<clip>, MAR|<clip>), verify.json, previews in /root/agb/native/previews/
import bpy, sys, os, json, math, time
import numpy as np
from mathutils import Vector
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import scene as SC
from body import Body
import positions as PO
import positions2                                      # registers the remaining positions into PO.CLIPS

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
only = [a for a in argv if not a.startswith("--")]
only = only[0].split(",") if only else list(PO.CLIPS)
RENDER = "--norender" not in argv
NAT = SC.NAT; PRE = NAT + "/previews"; os.makedirs(PRE, exist_ok=True)
def log(*a): print("[anim]", *a, flush=True)
t0 = time.time()

objs = SC.load_scene(room=True)
if set(only) != set(PO.CLIPS) and os.path.exists(NAT + "/anim.blend"):     # partial run: keep the other clips' actions
    with bpy.data.libraries.load(NAT + "/anim.blend", link=False) as (src, dst):
        dst.actions = [a for a in src.actions if "|" in a and a.split("|", 1)[1] not in only]
    for a in dst.actions:
        if a is not None: a.use_fake_user = True
V = Body(objs["VivianRig"], objs["vivian_body"], "vivian")
M = Body(objs["MarcusRig"], objs["marcus_body"], "marcus")
viv_meas = json.load(open(NAT + "/vivian_measure.json")); mar_meas = json.load(open(NAT + "/marcus_measure.json"))

# ---------------------------------------------------------------- landmarks from her rest mesh
body = objs["vivian_body"]
B = np.array([tuple(v.co) for v in body.data.vertices], np.float32)
def attr(name):
    a = body.data.attributes.get(name); v = np.empty(len(a.data), np.float32); a.data.foreach_get("value", v); return v
clit = B[np.argmax(attr("v_clit"))]
V.add_landmark("clit", "下半身", Vector(clit))
for s, sx in ((".L", 1), (".R", -1)):
    n0 = V.rig.rest["乳親" + s].translation
    sel = (np.sign(B[:, 0]) == sx) & (np.abs(B[:, 2] - (n0.z - 0.10)) < 0.09) & (np.abs(B[:, 0] - sx * 0.09) < 0.07)
    nip = B[sel][np.argmin(B[sel][:, 1])]
    V.add_landmark("nipple" + s, "乳親" + s, Vector(nip))
    hz = V.rig.rest["下半身"].translation.z - 0.05
    sel = (np.abs(B[:, 2] - hz) < 0.02) & (np.sign(B[:, 0]) == sx)
    hip = B[sel][np.argmax(np.abs(B[sel][:, 0]))]
    V.add_landmark("hip" + s, "下半身", Vector(hip))
def mouth_point(teeth_obj):
    T = np.array([tuple(v.co) for v in teeth_obj.data.vertices], np.float32)
    f = T[(np.abs(T[:, 0]) < 0.01)]
    p = f[np.argmin(f[:, 1])]
    return Vector((0.0, float(p[1]) - 0.011, float(p[2]) - 0.004))
V.add_landmark("mouth", "頭", mouth_point(objs["vivian_teeth"]))
M.add_landmark("mouth", "頭", mouth_point(objs["marcus_teeth"]))
MBr = np.array([tuple(v.co) for v in objs["marcus_body"].data.vertices], np.float32)
c2 = M.rig.rest["上半身2"].translation
for s, sx in ((".L", 1), (".R", -1)):
    sel = (np.abs(MBr[:, 0] - sx * 0.10) < 0.025) & (np.abs(MBr[:, 2] - (c2.z + 0.24)) < 0.03)
    M.add_landmark("chest" + s, "上半身2", Vector(MBr[sel][np.argmin(MBr[sel][:, 1])]))
    hz = (M.rig.rest["足" + s].translation.z + M.rig.rest["ひざ" + s].translation.z) / 2
    sel = (np.abs(MBr[:, 0] - sx * 0.12) < 0.03) & (np.abs(MBr[:, 2] - hz) < 0.03)
    M.add_landmark("thigh" + s, "足" + s, Vector(MBr[sel][np.argmin(MBr[sel][:, 1])]))
MBb = MBr
c2 = M.rig.rest["上半身2"].translation
for s, sx in ((".L", 1), (".R", -1)):                  # his upper back (for her embracing hands)
    sel = (np.abs(MBb[:, 0] - sx * 0.11) < 0.03) & (np.abs(MBb[:, 2] - (c2.z + 0.22)) < 0.03)
    M.add_landmark("back" + s, "上半身2", Vector(MBb[sel][np.argmax(MBb[sel][:, 1])]))
ctx = PO.Ctx(V, M, viv_meas, mar_meas)
log(f"scene ready ({time.time()-t0:.0f}s): clit {np.round(clit,3)}, canal {np.round(viv_meas['vagina']['entrance'],3)} axis {np.round(viv_meas['vagina']['axis'],2)}")

# ---------------------------------------------------------------- regions for verification
her_allow = SC.attr_mask(body, ("v_muc", "v_min", "v_maj", "v_cav", "v_clit", "v_pub", "v_anus"), 0.02)
her_hand = SC.vgroup_mask(body, [n for n in body.vertex_groups.keys() if n.startswith(("手首", "親指", "人指", "中指", "薬指", "小指"))], 0.3)
mb = objs["marcus_body"]
his_allow = SC.vgroup_mask(mb, ("竿根", "竿中", "竿先", "玉袋"), 0.2)
his_hand = SC.vgroup_mask(mb, [n for n in mb.vertex_groups.keys() if n.startswith(("手首", "親指", "人指", "中指", "薬指", "小指"))], 0.3)
MB = np.array([tuple(v.co) for v in mb.data.vertices], np.float32)
her_bones = SC.dominant_bone(body); his_bones = SC.dominant_bone(mb)
V.core = np.array([b in SC.CORE_BONES for b in her_bones]); M.core = np.array([b in SC.CORE_BONES for b in his_bones])
his_head = np.array([b in ("頭", "首") for b in his_bones])
HM = np.array([tuple(v.co) for v in mb.data.vertices], np.float32)
hh0 = M.rig.rest["頭"].translation
his_mouth = his_head & (HM[:, 2] < hh0.z + 0.06) & (HM[:, 1] < hh0.y - 0.05)
root0 = Vector(mb.parent.data.bones["竿根"].head_local)
his_allow |= np.linalg.norm(MB - np.array(root0), axis=1) < 0.07          # pubic mound around the shaft root
his_shaft = SC.vgroup_mask(mb, ("竿根", "竿中", "竿先", "玉袋"), 0.2)
E0 = np.array(viv_meas["vagina"]["entrance"], np.float32)
her_vulva_zone = np.linalg.norm(B - E0, axis=1) < 0.06                    # inner thighs / buttocks squeezing the shaft
# pelvis-to-pelvis contact (her buttocks / inner thighs on his groin, hips, upper thighs): flesh that compresses in
# every penetrative position, so touching there is intended contact
her_seat = (np.linalg.norm(B - E0, axis=1) < 0.19) & np.array([b.startswith(("足", "下半身")) for b in SC.dominant_bone(body)])
his_seat = (np.linalg.norm(MB - np.array(root0), axis=1) < 0.22) & np.array([b.startswith(("足", "下半身", "上半身")) for b in SC.dominant_bone(mb)])

def keys_for(clip, t):
    sex = clip.split(".")[0] not in ("solo",)
    pen = sex and not clip.startswith(("oral", "lick"))
    if not pen: vo = 0.0
    elif clip.startswith(("cowgirl", "revcowgirl")) or not clip.endswith(".in"): vo = 1.0   # always inserted (rides / rub / embrace)
    else: vo = PO.ss(0.0, 0.12, t)
    viv = {"vag_open": vo}
    mar = {"aroused": 1.0 if sex and not clip.startswith("lick") else 0.0}     # lick: he lies on it, flaccid + tucked
    return viv, mar

def evaluate(clip, t):
    viv, mar = keys_for(clip, t)
    SC.set_keys("vivian_", viv); SC.set_keys("marcus_", mar)
    SC.apply_rigs(V, M)
    A, Pa = SC.evaluated(body); Bm, Pb = SC.evaluated(mb)
    return A, Pa, Bm, Pb

def verify(clip, t):
    A, Pa, Bm, Pb = evaluate(clip, t)
    allowA = her_allow | her_hand; allowB = his_allow | his_hand
    if clip.startswith("oral"): allowA = allowA | mouth_mask
    if clip.startswith("lick"): allowB = allowB | his_mouth
    pen = not clip.startswith(("oral", "lick", "solo"))
    extra = [(her_vulva_zone, his_shaft), (her_seat, his_seat | his_shaft)] if pen else []
    pairs, bad, depth, worst = SC.interpenetration(A, Pa, allowA, Bm, Pb, allowB, boneA=her_bones, boneB=his_bones,
                                                   handA=her_hand, handB=his_hand, extra=extra)
    her_min = float(A[:, 2].min()); his_min = float(Bm[:, 2].min())
    low = (her_bones[int(A[:, 2].argmin())], his_bones[int(Bm[:, 2].argmin())])
    return dict(t=round(t, 2), pairs=pairs, bad=bad, depth_mm=round(depth * 1000, 1), worst=[f"{a}x{b}:{n}" for (a, b), n in worst],
                her_floor_mm=round((her_min - SC.BED_TOP) * 1000, 1), his_floor_mm=round((his_min - SC.BED_TOP) * 1000, 1), lowest=list(low))

def _score():
    r_ = verify(PO.CURRENT, 0.5)
    return r_["bad"] + 3.0 * r_["depth_mm"] + 8.0 * max(0.0, -10.0 - r_["her_floor_mm"]) + 8.0 * max(0.0, -10.0 - r_["his_floor_mm"])
PO.SCORE = _score

mouth_mask = np.zeros(len(B), bool)
hb = V.rig.rest["頭"].translation
mouth_mask |= (np.abs(B[:, 0]) < 0.03) & (B[:, 2] < hb.z + 0.02) & (B[:, 2] > hb.z - 0.08) & (B[:, 1] < hb.y - 0.06)

# ---------------------------------------------------------------- render setup (workbench, fast)
def setup_render():
    sc = bpy.context.scene
    try:
        sc.render.engine = "BLENDER_WORKBENCH"
        sh = sc.display.shading; sh.light = "STUDIO"; sh.color_type = "TEXTURE"; sh.show_shadows = True; sh.show_cavity = True
    except Exception:
        sc.render.engine = "CYCLES"; sc.cycles.samples = 8
    sc.render.resolution_x, sc.render.resolution_y = 960, 540
    cam = bpy.data.cameras.get("prev") or bpy.data.cameras.new("prev"); cam.lens = 30
    co = bpy.data.objects.get("prev_cam") or bpy.data.objects.new("prev_cam", cam)
    if co.name not in sc.objects: sc.collection.objects.link(co)
    sc.camera = co; return co
CAM = setup_render() if RENDER else None
def render(path, target, eye):
    CAM.location = eye; CAM.rotation_euler = (Vector(target) - Vector(eye)).to_track_quat("-Z", "Y").to_euler()
    bpy.context.scene.render.filepath = path; bpy.ops.render.render(write_still=True)

# ---------------------------------------------------------------- bake loop
report = json.load(open(NAT + "/verify.json")) if os.path.exists(NAT + "/verify.json") else {}
for clip in only:
    fn, frames, kind = PO.CLIPS[clip]
    tc = time.time()
    acts = {}
    for who, body_ in (("VIV", V), ("MAR", M)):
        nm = f"{who}|{clip}"
        old = bpy.data.actions.get(nm)
        if old: bpy.data.actions.remove(old)
        act = bpy.data.actions.new(nm); act.use_fake_user = True
        body_.arm.animation_data_create(); body_.arm.animation_data.action = act; acts[who] = act
    PO.CURRENT = clip
    fn(ctx, 0.5)                                       # best_of choices are made once, at mid-stroke
    prev = {"VIV": None, "MAR": None}; worst_flip = (0.0, None)
    for f in range(frames):
        t = f / (frames - 1) if kind != "loop" else f / (frames - 1)
        fn(ctx, t)
        for who, body_ in (("VIV", V), ("MAR", M)):
            snap = body_.rig.snapshot()
            if prev[who] is not None:
                w = SC.flips(prev[who], snap)
                if w[0] > worst_flip[0]: worst_flip = (w[0], f"{who}:{w[1]}")
            prev[who] = snap
            body_.rig.key(f + 1)
    samples = [0.0, 0.25, 0.5, 0.75, 1.0] if kind != "pose" else [0.0]
    res = []
    for t in samples:
        fn(ctx, t); res.append(verify(clip, t))
    rep = dict(frames=frames, kind=kind, samples=res, worst_flip_deg=round(worst_flip[0], 1), flip_bone=worst_flip[1],
               max_bad_pairs=max(r["bad"] for r in res), max_depth_mm=max(r["depth_mm"] for r in res))
    report[clip] = rep
    log(f"{clip}: {frames}f in {time.time()-tc:.0f}s | bad pairs max {rep['max_bad_pairs']} depth {rep['max_depth_mm']} mm | "
        f"floors her {[r['her_floor_mm'] for r in res]} his {[r['his_floor_mm'] for r in res]} | flip {rep['worst_flip_deg']} {rep['flip_bone']}")
    for r_ in res:
        if r_["bad"]: log(f"   t={r_['t']}: bad {r_['bad']} depth {r_['depth_mm']} mm worst {r_['worst']}")
    if RENDER:
        fn(ctx, 0.6 if kind == "depth" else 0.25)
        evaluate(clip, 0.6)
        pel = V.rig.head("下半身")
        for tag, eye in (("side", pel + Vector((1.9, -0.2, 0.75))), ("front", pel + Vector((0.35, -2.1, 0.9))), ("top", pel + Vector((0.6, -0.4, 2.2)))):
            render(f"{PRE}/{clip.replace('.', '_')}_{tag}.png", pel + Vector((0, 0.15, 0.05)), eye)
json.dump(report, open(NAT + "/verify.json", "w"), indent=1)
for b in (V, M): b.arm.animation_data.action = None
for lib in list(bpy.data.libraries): bpy.data.libraries.remove(lib)      # appended actions are local; drop the source ref
bpy.ops.wm.save_as_mainfile(filepath=NAT + "/anim_new.blend"); os.replace(NAT + "/anim_new.blend", NAT + "/anim.blend")
log(f"done ({time.time()-t0:.0f}s), saved anim.blend")
