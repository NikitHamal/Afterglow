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
import positions3                                      # more acts: handjob, titjob, face-riding, cuddle, make-out
import positions4                                      # story clips: stand.idle, sit.bed

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
        dst.actions = [a for a in src.actions if "|" in a and a.split("|", 1)[1] not in only and a.split("|", 1)[1] in PO.CLIPS]
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
LIMB = ("腕", "ひじ", "手首", "親指", "人指", "中指", "薬指", "小指")
her_limb = np.array([b.startswith(LIMB) for b in her_bones]); his_limb = np.array([b.startswith(LIMB) for b in his_bones])
def side_mask(bones, base, side): return np.array([b.startswith(LIMB[2:]) and b.endswith(side) for b in bones]) & base
EFFECTORS = {"vivian": {"hand.L": side_mask(her_bones, her_hand, ".L"), "hand.R": side_mask(her_bones, her_hand, ".R")},
             "marcus": {"hand.L": side_mask(his_bones, his_hand, ".L"), "hand.R": side_mask(his_bones, his_hand, ".R")}}
V.core = np.array([b in SC.CORE_BONES for b in her_bones]); M.core = np.array([b in SC.CORE_BONES for b in his_bones])
his_head = np.array([b in ("頭", "首") for b in his_bones])
HM = np.array([tuple(v.co) for v in mb.data.vertices], np.float32)
hh0 = M.rig.rest["頭"].translation
his_mouth = his_head & (HM[:, 2] < hh0.z + 0.06) & (HM[:, 1] < hh0.y - 0.05)
root0 = Vector(mb.parent.data.bones["竿根"].head_local)
his_allow |= np.linalg.norm(MB - np.array(root0), axis=1) < 0.07          # pubic mound around the shaft root
his_shaft = SC.vgroup_mask(mb, ("竿根", "竿中", "竿先", "玉袋"), 0.2)
her_breast = SC.vgroup_mask(body, ("乳親.L", "乳親.R"), 0.3)
TRUNK = ("上半身", "上半身2", "乳親", "腕", "肩", "首")
her_trunk = np.array([b.startswith(TRUNK) for b in SC.dominant_bone(body)])
his_trunk = np.array([b.startswith(TRUNK) for b in SC.dominant_bone(mb)])
E0 = np.array(viv_meas["vagina"]["entrance"], np.float32)
her_vulva_zone = np.linalg.norm(B - E0, axis=1) < 0.06                    # inner thighs / buttocks squeezing the shaft
# pelvis-to-pelvis contact (her buttocks / inner thighs on his groin, hips, upper thighs): flesh that compresses in
# every penetrative position, so touching there is intended contact
her_seat = (np.linalg.norm(B - E0, axis=1) < 0.26) & np.array([b.startswith(("足", "下半身")) for b in SC.dominant_bone(body)])
his_seat = (np.linalg.norm(MB - np.array(root0), axis=1) < 0.28) & np.array([b.startswith(("足", "下半身", "上半身")) for b in SC.dominant_bone(mb)])

# room furniture the bodies must never enter (the sheet/mattress are the support: checked by the floors instead)
ROOM_HIT = ("headboard", "pillow", "duvet", "nightstand", "lamp", "bed_base", "candle", "book", "wine", "drawer", "knob")
ROOM = None
rv, rp, rspan = [], [], []
for o in list(bpy.context.scene.objects):
    if o.type == "MESH" and o.name.startswith(ROOM_HIT):
        Vo, Po = SC.evaluated(o); base = len(rv); f0 = len(rp)
        rv.extend(map(tuple, Vo)); rp.extend(tuple(base + i for i in f) for f in Po); rspan.append((f0, len(rp), o.name))
def room_name(fi):
    for a_, b_, n_ in rspan:
        if a_ <= fi < b_: return n_
    return "?"
if rv:
    from mathutils.bvhtree import BVHTree as _BVH
    ROOM = _BVH.FromPolygons(rv, rp)
log(f"room furniture for checks: {len(rp)} faces" if ROOM else "no room loaded: furniture checks skipped")
PLAY_Y1 = SC.HEAD_Y - 0.24                                           # pillows stand beyond this line
hair_o = objs.get("vivian_hair")
hair_head = None
if hair_o is not None:
    hg = hair_o.vertex_groups.get("頭")
    hair_head = np.zeros(len(hair_o.data.vertices), bool)
    if hg is not None:
        for v in hair_o.data.vertices:
            for g in v.groups:
                if g.group == hg.index and g.weight > 0.99: hair_head[v.index] = True

NONPEN = ("oral", "lick", "handjob", "titjob", "facesit", "cuddle", "kiss", "stand", "sit")
AROUSED = {"lick": 0.0, "cuddle": 0.0, "kiss": 0.35, "facesit": 0.8, "stand": 0.0, "sit": 0.0}
def keys_for(clip, t):
    sex = clip.split(".")[0] not in ("solo",)
    pen = sex and not clip.startswith(NONPEN)
    if not pen: vo = 0.0
    elif clip.startswith(("cowgirl", "revcowgirl")) or not clip.endswith(".in"): vo = 1.0   # always inserted (rides / rub / embrace)
    else: vo = PO.ss(0.0, 0.12, t)
    viv = {"vag_open": vo}
    mar = {"aroused": AROUSED.get(clip.split(".")[0], 1.0) if sex else 0.0}   # lick: he lies on it, flaccid + tucked
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
    if clip.startswith(("lick", "facesit")): allowB = allowB | his_mouth
    if clip.startswith("kiss"): allowA = allowA | mouth_mask; allowB = allowB | his_mouth
    pen = not clip.startswith(("solo",) + NONPEN)
    extra = [(her_vulva_zone, his_shaft), (her_seat, his_seat | his_shaft)] if pen else []
    if clip.startswith("titjob"): extra = [(her_breast, his_shaft), (her_breast, his_seat)]
    if clip.startswith("facesit"): extra = [(her_seat, his_head)]
    if clip.startswith(("cuddle", "kiss", "sit", "stand")): extra = [(her_trunk, his_trunk), (her_seat, his_seat)]
    pairs, bad, depth, worst = SC.interpenetration(A, Pa, allowA, Bm, Pb, allowB, boneA=her_bones, boneB=his_bones,
                                                   handA=her_hand, handB=his_hand, extra=extra)
    if os.environ.get("DIAG") and bad:
        ia = np.array(sorted({k for i, _ in SC.LAST_BAD for k in Pa[i]})); ib = np.array(sorted({k for _, j in SC.LAST_BAD for k in Pb[j]}))
        dA = np.linalg.norm(B[ia] - E0, axis=1); dB = np.linalg.norm(MB[ib] - np.array(root0), axis=1)
        log(f"   DIAG {clip} t={t}: her dist-from-entrance cm p10/50/90 {np.round(np.percentile(dA, [10, 50, 90]) * 100, 1)} "
            f"his dist-from-root {np.round(np.percentile(dB, [10, 50, 90]) * 100, 1)} | her world z-bed cm {np.round(np.percentile(A[ia, 2] - SC.BED_TOP, [10, 50, 90]) * 100, 1)} "
            f"x {np.round(np.percentile(np.abs(A[ia, 0]), [10, 50, 90]) * 100, 1)}")
    room_pairs = 0; room_hit = {}
    if ROOM is not None:
        from mathutils.bvhtree import BVHTree as _BVH
        for P_, F_ in ((A, Pa), (Bm, Pb)):
            ov = ROOM.overlap(_BVH.FromPolygons([tuple(v) for v in P_], F_)); room_pairs += len(ov)
            for fi, _ in ov: n_ = room_name(fi); room_hit[n_] = room_hit.get(n_, 0) + 1
    hair_floor = hair_his = None
    if hair_o is not None:
        Hv, Hp = SC.evaluated(hair_o)
        hair_floor = round((float(Hv[hair_head, 2].min()) - SC.BED_TOP) * 1000, 1) if hair_head.any() else None
        from mathutils.bvhtree import BVHTree as _BVH
        Hp_rigid = [f for f in Hp if all(hair_head[i] for i in f)]
        hair_his = len(_BVH.FromPolygons([tuple(v) for v in Hv], Hp_rigid).overlap(_BVH.FromPolygons([tuple(v) for v in Bm], Pb))) if Hp_rigid else 0
    from mathutils.bvhtree import BVHTree as _BVH
    surf = {"vivian": (A, Pa, her_limb, her_bones, B), "marcus": (Bm, Pb, his_limb, his_bones, MB)}
    full = {w: _BVH.FromPolygons([tuple(v) for v in X], F) for w, (X, F, _, _, _) in surf.items()}
    trunk = {w: _BVH.FromPolygons([tuple(v) for v in X], [f for f in F if not any(lm[i] for i in f)]) for w, (X, F, lm, _, _) in surf.items()}
    contacts = []
    for who, effs in EFFECTORS.items():
        X = surf[who][0]
        for eff, msk in effs.items():
            idx = np.where(msk)[0][::2]
            if not len(idx): continue
            for tgt in ("vivian", "marcus"):
                if eff == "mouth" and tgt == who: continue
                bvh = trunk[tgt] if tgt == who else full[tgt]
                best = (1e9, None, None)
                for i in idx:
                    loc, nrm_, fi, dist_ = bvh.find_nearest(Vector(X[i]))
                    if loc is None or dist_ > 0.03: continue
                    d = (Vector(X[i]) - loc).dot(nrm_)
                    if d < best[0]: best = (d, i, loc)
                if best[1] is None or best[0] > 0.025: continue
                Xt, _, lmt, bt, Rt = surf[tgt]
                cand = np.where(~lmt)[0]                       # a region of the body, never the hand itself
                j = int(cand[np.argmin(np.linalg.norm(Xt[cand] - np.array(best[2]), axis=1))])
                contacts.append(dict(char=who, effector=eff, vert=int(best[1]), gap_mm=round(best[0] * 1000, 1),
                                     target=dict(char=tgt, bone=bt[j], region=region(tgt, bt[j], Rt[j]))))
    rom = max(max(b_.rig.rom_excess(n) for n in b_.rig.ROM) for b_ in (V, M))
    her_min = float(A[:, 2].min()); his_min = float(Bm[:, 2].min())
    low = (her_bones[int(A[:, 2].argmin())], his_bones[int(Bm[:, 2].argmin())])
    return dict(t=round(t, 2), pairs=pairs, bad=bad, depth_mm=round(depth * 1000, 1), worst=[f"{a}x{b}:{n}" for (a, b), n in worst],
                her_floor_mm=round((her_min - SC.BED_TOP) * 1000, 1), his_floor_mm=round((his_min - SC.BED_TOP) * 1000, 1), lowest=list(low),
                extent_y=[round(float(min(A[:, 1].min(), Bm[:, 1].min())), 3), round(float(max(A[:, 1].max(), Bm[:, 1].max())), 3)],
                extent_x=[round(float(min(A[:, 0].min(), Bm[:, 0].min())), 3), round(float(max(A[:, 0].max(), Bm[:, 0].max())), 3)],
                room_pairs=room_pairs, room_hit=room_hit, hair_floor_mm=hair_floor, hair_vs_him=hair_his,
                pelvic_depth_mm=round(SC.LAST_ZONE_DEPTH * 1000, 1), contacts=contacts, rom_excess_deg=round(rom, 1),
                beyond_play_mm=round(max(0.0, float(max(A[:, 1].max(), Bm[:, 1].max())) - PLAY_Y1) * 1000, 1))

PRE_T = 0.85                                                       # layout choices are made near full depth...
def _score():
    sc = 0.0
    clip = PO.CURRENT; fn_, _, kind_ = PO.CLIPS[clip]
    for t_ in ((0.0, PRE_T) if kind_ == "depth" else (0.5,)):        # ...and must also hold at the top of the stroke
        if t_ != PRE_T and kind_ == "depth":
            snaps = [b_.rig.snapshot() for b_ in (V, M)]
            PO.SCORE_T = t_; fn_(ctx, t_); PO.SCORE_T = None
            sc += _score_at(clip, t_)
            for b_, sn in zip((V, M), snaps): b_.rig.restore(sn)
        else:
            sc += _score_at(clip, t_)
    return sc
def _score_at(clip, t_):
    r_ = verify(clip, t_)
    sc = r_["bad"] + 3.0 * r_["depth_mm"] + 8.0 * max(0.0, -10.0 - r_["her_floor_mm"]) + 8.0 * max(0.0, -10.0 - r_["his_floor_mm"])
    sc += 4.0 * r_["room_pairs"] + 2.0 * r_["beyond_play_mm"] + 2.0 * (r_["hair_vs_him"] or 0)
    sc += 4.0 * max(0.0, r_["pelvic_depth_mm"] - 8.0)             # flesh may press, not sink through
    sc += 3.0 * r_["rom_excess_deg"]
    if r_["hair_floor_mm"] is not None: sc += 4.0 * max(0.0, -15.0 - r_["hair_floor_mm"])
    return sc
PO.SCORE = _score

mouth_mask = np.zeros(len(B), bool)
hb = V.rig.rest["頭"].translation
mouth_mask |= (np.abs(B[:, 0]) < 0.03) & (B[:, 2] < hb.z + 0.02) & (B[:, 2] > hb.z - 0.08) & (B[:, 1] < hb.y - 0.06)
lip_v = V.lm("mouth"); lip_m = M.lm("mouth")
EFFECTORS["vivian"]["mouth"] = np.linalg.norm(B - np.array(V.rig.rest["頭"].translation) * 0 - np.array(lip_v), axis=1) < 0.025
EFFECTORS["marcus"]["mouth"] = np.linalg.norm(MB - np.array(lip_m), axis=1) < 0.025
def region(who, bone, P_rest):
    if who == "vivian":
        if np.linalg.norm(P_rest - E0) < 0.045: return "vulva"
        if bone.startswith("乳親"): return "breast" + bone[-2:]
    if who == "marcus" and bone.startswith(("竿", "玉袋")): return "genitals"
    for k, v in (("下半身", "hips"), ("上半身2", "chest"), ("上半身", "belly"), ("足", "thigh"), ("ひざ", "shin"), ("足首", "foot"),
                 ("頭", "head"), ("首", "neck"), ("腕", "upper_arm"), ("ひじ", "forearm"), ("肩", "shoulder")):
        if bone.startswith(k): return v + (bone[-2:] if bone[-2:] in (".L", ".R") else "")
    return bone

# ---------------------------------------------------------------- render setup (workbench, fast)
def setup_render():
    sc = bpy.context.scene
    if os.environ.get("AG_PREVIEW_ENGINE") == "CYCLES":             # headless boxes without GL: fast denoised Cycles
        sc.render.engine = "CYCLES"; sc.cycles.device = "CPU"; sc.cycles.samples = 12; sc.cycles.use_denoising = True
        sc.render.resolution_x, sc.render.resolution_y = 960, 540
        if sc.world is None: sc.world = bpy.data.worlds.new("prev")
        sc.world.use_nodes = True; sc.world.node_tree.nodes["Background"].inputs[0].default_value = (0.6, 0.6, 0.62, 1)
        sc.world.node_tree.nodes["Background"].inputs[1].default_value = 1.0
        cam = bpy.data.cameras.get("prev") or bpy.data.cameras.new("prev"); cam.lens = 30
        co = bpy.data.objects.get("prev_cam") or bpy.data.objects.new("prev_cam", cam)
        if co.name not in sc.objects: sc.collection.objects.link(co)
        sc.camera = co; return co
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
    for b_ in (V, M): b_.rig._last_hinge = {}; b_.rig._last_twist = {}; b_.rig._last_curl = {}; b_.rig._last_pole = {}   # continuity within a clip
    fn(ctx, PRE_T if kind == "depth" else 0.5)       # best_of choices are made once (near full depth)
    fn(ctx, 0.0)                                       # ...then the hinge memory starts from the first frame
    prev = {"VIV": None, "MAR": None}; worst_flip = (0.0, None)
    for f in range(frames):
        t = f / (frames - 1) if kind != "loop" else f / (frames - 1)
        fn(ctx, t)
        for b_ in (V, M): b_.rig.limit_all()                  # anatomical range of motion, always
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
        fn(ctx, t)
        for b_ in (V, M): b_.rig.limit_all()
        res.append(verify(clip, t))
    rep = dict(frames=frames, kind=kind, samples=res, worst_flip_deg=round(worst_flip[0], 1), flip_bone=worst_flip[1],
               max_bad_pairs=max(r["bad"] for r in res), max_depth_mm=max(r["depth_mm"] for r in res))
    # contact tracks for the engine: one per (character, effector, target character), offset in the effector bone's
    # rest frame (bone-local, same convention as clips.json fpv.offset_local), with the gap range over the samples
    tracks = {}
    for r_ in res:
        for c in r_["contacts"]:
            k_ = (c["char"], c["effector"], c["target"]["char"])
            tr = tracks.setdefault(k_, dict(char=c["char"], effector=c["effector"], target=c["target"], gaps=[], ts=[], verts=[]))
            tr["gaps"].append(c["gap_mm"]); tr["ts"].append(r_["t"]); tr["verts"].append(c["vert"])
    out_tracks = []
    for (who, eff, tgt), tr in tracks.items():
        vi = tr["verts"][len(tr["verts"]) // 2]
        bones_, Rest = (her_bones, B) if who == "vivian" else (his_bones, MB)
        bn = bones_[vi]; body_ = V if who == "vivian" else M
        loc_ = body_.rig.rest[bn].inverted() @ Vector(Rest[vi])
        rad = 0.02 if eff == "mouth" else (0.012 if bn.startswith(("親指", "人指", "中指", "薬指", "小指")) else 0.035)
        out_tracks.append(dict(char=who, effector=eff, bone=bn, offset=[round(loc_.x, 4), round(loc_.y, 4), round(loc_.z, 4)], radius=rad,
                               target=tr["target"], gap_mm=[min(tr["gaps"]), max(tr["gaps"])], t=[min(tr["ts"]), max(tr["ts"])]))
    rep["contacts"] = out_tracks
    rep["max_rom_excess_deg"] = max(r_["rom_excess_deg"] for r_ in res)
    report[clip] = rep
    log(f"{clip}: {frames}f in {time.time()-tc:.0f}s | bad pairs max {rep['max_bad_pairs']} depth {rep['max_depth_mm']} mm | "
        f"floors her {[r['her_floor_mm'] for r in res]} his {[r['his_floor_mm'] for r in res]} | flip {rep['worst_flip_deg']} {rep['flip_bone']}")
    for r_ in res:
        if r_["bad"]: log(f"   t={r_['t']}: bad {r_['bad']} depth {r_['depth_mm']} mm worst {r_['worst']}")
    if RENDER:
        pt = float(os.environ.get("AG_PREVIEW_T", "0.6"))
        fn(ctx, pt if kind == "depth" else 0.25)
        evaluate(clip, pt)
        pel = V.rig.head("下半身")
        views = (("side", pel + Vector((1.9, -0.2, 0.75))), ("front", pel + Vector((0.35, -2.1, 0.9))), ("top", pel + Vector((0.6, -0.4, 2.2))),
                 ("feet", pel + Vector((0.05, -2.4, 0.35))), ("close", pel + Vector((0.9, -0.9, 0.45))))
        want = os.environ.get("AG_PREVIEW_VIEWS", "side,front,top").split(",")
        for tag, eye in views:
            if tag in want: render(f"{PRE}/{clip.replace('.', '_')}_{tag}.png", pel + Vector((0, 0.15, 0.05)), eye)
json.dump(report, open(NAT + "/verify.json", "w"), indent=1)
for b in (V, M): b.arm.animation_data.action = None
for lib in list(bpy.data.libraries): bpy.data.libraries.remove(lib)      # appended actions are local; drop the source ref
bpy.ops.wm.save_as_mainfile(filepath=NAT + "/anim_new.blend"); os.replace(NAT + "/anim_new.blend", NAT + "/anim.blend")
log(f"done ({time.time()-t0:.0f}s), saved anim.blend")
