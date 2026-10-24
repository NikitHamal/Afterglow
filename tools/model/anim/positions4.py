# Story-mode clips (loops, both characters, the same world as the acts): standing at talking distance in
# contrapposto, the standing embrace and kiss, sitting side by side on the bed's edge, the seated kiss.
# Registered into positions.CLIPS on import. Rules: limbs by their real side (never the L/R label), feet flat on
# the room floor or hanging free of the bed frame, hands on the partner's real skin (cup), heads within their range.
import math
import numpy as np
from mathutils import Vector, Matrix
import positions as P
from positions import V3, nrm, surface, cup, lift_out
from scene import BED_TOP, MAT_W
import scene as SC

UP = V3(0, 0, 1)
_MASK = {}
import os as _os
_DBG = bool(_os.environ.get("AG_KISSDBG"))

def _mask(body, kind):
    """vertex masks for settling: 'all' (standing on the floor) or 'seat' (pelvis/buttocks on the mattress)"""
    k = (body.obj.name, kind)
    if k not in _MASK:
        if kind == "all": _MASK[k] = np.ones(len(body.obj.data.vertices), bool)
        else: _MASK[k] = np.array([b == "下半身" for b in SC.dominant_bone(body.obj)])
    return _MASK[k]

def flat(v): return nrm(V3(v.x, v.y, 0))
def left_of(face): return UP.cross(flat(face)).normalized()          # Z-up: up x forward = the body's left
def yaw(v, deg):
    return Matrix.Rotation(math.radians(deg), 3, "Z") @ Vector(v)

def lr(body, face):
    """[(".L"/".R", +1 = on the body's left of `face`, -1 = right)] by where the limb really is"""
    r = body.rig; c = r.head("下半身"); left = left_of(face)
    return [(s, 1.0 if (r.head("足" + s) - c).dot(left) >= 0 else -1.0) for s in (".L", ".R")]

def arm_side(body, face):
    r = body.rig; c = r.head("上半身2"); left = left_of(face)
    return [(s, 1.0 if (r.head("腕" + s) - c).dot(left) >= 0 else -1.0) for s in (".L", ".R")]

# ---------------------------------------------------------------- standing
def stand(body, loc_xy, face, weight=0.6, breath=0.0, chest_lean=0.0, feet="normal"):
    """standing on the room floor at loc_xy facing `face`, contrapposto: weight on one leg (+1 her/his right,
    -1 left), the hips shifted over it with the free hip dropped, the free knee soft and that foot a little ahead,
    the shoulders countering; chest_lean (deg, + forward) for leaning in. feet: "normal", "wide" (toes out, an
    embrace's base) or "stagger" (stance foot back, the free foot forward on the midline: between a partner's feet)"""
    r = body.rig; r.reset()
    f = flat(face); left = left_of(f)
    w = max(-1.0, min(1.0, weight))
    up_p = nrm(UP + left * math.tan(math.radians(4.0)) * w)          # free hip drops: the pelvis rolls toward it
    body.place(V3(loc_xy[0], loc_xy[1], 1.0) - left * 0.025 * w, f, up_p)
    a = math.radians(chest_lean + 1.5 * breath)
    up_c = nrm(UP - left * math.tan(math.radians(2.5)) * w)           # shoulders counter the hips
    body.spine(forward=nrm(f - UP * 0.02), up=nrm(UP - left * math.tan(math.radians(1.0)) * w),
               chest_forward=nrm(f * math.cos(a) - up_c * math.sin(a)), chest_up=nrm(up_c * math.cos(a) + f * math.sin(a)))
    SC.settle(body, target_z=0.0, sink=0.0, region=_mask(body, "all"))
    for s, sx in lr(body, f):
        H = r.head("足" + s)
        stance = (sx < 0) == (w > 0)                                   # +w: the right leg carries the weight
        c = r.head("下半身"); hw = abs((H - c).dot(left))                  # hip half-width
        if feet == "wide":                                             # an embrace's base: feet apart, toes out
            A = V3(H.x, H.y, 0.075) + left * sx * 0.07 - f * 0.02
            body.leg_ik(s, A, H + f * 0.5 + left * sx * 0.12 - UP * 0.1)
            body.foot(s, nrm(f + left * sx * 0.55), UP)
        elif feet == "stagger" and stance:                             # weight on the back foot
            A = V3(H.x, H.y, 0.075) - f * 0.07 - left * sx * 0.005
            body.leg_ik(s, A, H + f * 0.5 - UP * 0.1)
            body.foot(s, nrm(f + left * sx * 0.30), UP)
        elif feet == "stagger":                                        # the free foot forward on the midline, heel light
            A = V3(H.x, H.y, 0.085) + f * 0.10 - left * sx * hw
            body.leg_ik(s, A, H + f * 0.6)
            body.foot(s, nrm(f - UP * 0.05), nrm(UP + f * 0.05))
        elif stance:
            A = V3(H.x, H.y, 0.075) - left * sx * 0.01 + f * 0.01
            body.leg_ik(s, A, H + f * 0.5 - UP * 0.1)
            body.foot(s, nrm(f + left * sx * 0.18), UP)
        else:                                                          # free leg: knee soft, foot ahead and out, heel light
            A = V3(H.x, H.y, 0.082) + f * 0.09 + left * sx * 0.035
            body.leg_ik(s, A, H + f * 0.6 + left * sx * 0.05)
            body.foot(s, nrm(f + left * sx * 0.30 - UP * 0.04), nrm(UP + f * 0.04))
    SC.settle(body, target_z=0.0, sink=0.0, region=_mask(body, "all"))
    return f, left

def relaxed_arms(body, f, left, swing=0.0, sides=None):
    """arms hanging with soft elbows (~25 deg), hands beside the thighs, palms in, fingers loosely curled"""
    r = body.rig; bvh = surface(body)
    for s, sx in arm_side(body, f):
        if sides is not None and s not in sides: continue
        sh = r.head("腕" + s); side = left * sx
        reach = 0.86 * (body.len["upper" + s] + body.len["fore" + s])        # a soft elbow, never locked
        W = sh + side * 0.07 + f * (0.06 + 0.02 * swing * sx) - UP * reach
        pole = sh - f * 0.30 + side * 0.10 - UP * 0.10
        def pose_hand(s=s, side=side):
            body.hand(s, palm=-side + f * 0.15, fingers=nrm(-UP + f * 0.18)); body.rig.limit("手首" + s); body.fingers(s, 0.38)
        body.arm_ik(s, W, pole); pose_hand()
        lift_out(body, s, bvh, W, side, pose_hand, pole, allow=0.006, iters=4)

def stand_idle(ctx, t):
    V, M = ctx.V, ctx.M
    ph = 2 * math.pi * t; breath = math.sin(ph)
    fv, lv = stand(V, (-0.42, -0.45), V3(0.85, 0.15, 0), weight=0.75 + 0.12 * math.sin(ph), breath=breath)
    fm, lm = stand(M, (0.42, -0.62), V3(-0.85, 0.2, 0), weight=-0.6 + 0.10 * math.sin(ph + 1.3), breath=math.sin(ph + 0.7))
    relaxed_arms(V, fv, lv, swing=0.3 * math.sin(ph)); relaxed_arms(M, fm, lm, swing=0.3 * math.sin(ph + 1.3))
    V.look(M.rig.head("頭") + V3(0, 0, -0.02)); M.look(V.rig.head("頭"))

# ---------------------------------------------------------------- the kiss (any posture): lips meet, noses pass
_HEADS = {}                                                             # last frame's neck/head solution per clip

def _apply_heads(b, base, face, p, roll, yaw_r=0.0):
    """neck/head of body b = base pose + (neck pitch, neck yaw, head pitch, head yaw) deg, head rolled `roll` and
    turned `yaw_r` to its own right (two faces cross at an angle: noses and brows pass)"""
    r = b.rig; r.restore(base)
    left = left_of(face); f = flat(face)
    r.rotate_world("首", Matrix.Rotation(math.radians(p[1]), 3, UP) @ Matrix.Rotation(math.radians(p[0]), 3, left))
    r.rotate_world("頭", Matrix.Rotation(math.radians(roll), 3, f) @ Matrix.Rotation(math.radians(p[3] - yaw_r), 3, UP)
                   @ Matrix.Rotation(math.radians(p[2]), 3, left))           # +roll about the facing = toward its right

def skull(b):
    """centre of the cranium: above and a little behind the head pivot, in the head's own frame"""
    return b.rig.head("頭") + b.up_dir("頭") * 0.075 - b.fwd_dir("頭") * 0.005

_LAYOUT = {}                                                            # per clip: the lean / distance decided once

def solve_heads(V, M, fv, fm, goal, key, roll=(20.0, 20.0), weight=(2.0, 1.0), yaw_r=(5.0, 5.0), clear=0.175):
    """neck + head of both found by coordinate descent on goal() (the lip gap, or cheek-on-chest), inside the
    neck/head ranges, crania `clear` apart (no skulls or brows through each other); warm-started from the last
    frame of the clip and held near it, so the solution glides. Returns goal() at the solution."""
    bases = (V.rig.snapshot(), M.rig.snapshot())
    p0 = list(_HEADS.get(key, [0.0] * 8)); p = list(p0)
    def cost(q):
        _apply_heads(V, bases[0], fv, q[:4], roll[0], yaw_r[0]); _apply_heads(M, bases[1], fm, q[4:], roll[1], yaw_r[1])
        c = goal() ** 2 * 1e4
        hd = (skull(V) - skull(M)).length                               # crania stay apart
        c += max(0.0, clear - hd) ** 2 * 4e5
        if key in _HEADS: c += 3e-3 * sum((a - b_) ** 2 for a, b_ in zip(q, p0))   # glide from the last frame
        for b in (V, M):
            for n in ("首", "頭"): c += b.rig.rom_excess(n) ** 2 * 0.05
        c += 2e-4 * (weight[0] * sum(x * x for x in q[:4]) + weight[1] * sum(x * x for x in q[4:]))
        return c
    best = cost(p)
    for step in (16.0, 8.0, 4.0, 2.0, 1.0, 0.5):
        improved = True
        while improved:
            improved = False
            for i in range(8):
                for sg in (1.0, -1.0):
                    q = list(p); q[i] += sg * step
                    c = cost(q)
                    if c < best - 1e-9: best, p, improved = c, q, True
    cost(p); _HEADS[key] = p
    for b in (V, M): b.rig.limit("首"); b.rig.limit("頭")
    return goal()

def lip_gap(V, M):
    return lambda: max(0.0, (V.lm("mouth") - M.lm("mouth")).length - 0.001)

def kiss_heads(V, M, fv, fm, key, nod=0.0):
    """the lips meet (1 mm), each head rolled ~17 deg to its own right so the noses pass, he bends down more than
    she reaches up; a slow nod keeps the kiss alive. Returns the lip gap (m)."""
    V.look(M.rig.head("頭"), up_hint=UP, neck_share=0.5)            # face each other; the roll comes in the solve
    M.look(V.rig.head("頭"), up_hint=UP, neck_share=0.5)
    g = solve_heads(V, M, fv, fm, lip_gap(V, M), key)
    if nod:
        V.rig.rotate_world("頭", Matrix.Rotation(math.radians(nod), 3, left_of(fv)))
        M.rig.rotate_world("頭", Matrix.Rotation(math.radians(-nod), 3, left_of(fm)))
    g = (V.lm("mouth") - M.lm("mouth")).length
    if _DBG: print("[kissdbg]", key, "gap", round(g * 1000, 1), "heads", round((V.rig.head("頭") - M.rig.head("頭")).length * 1000), flush=True)
    return g

def cheek_on_chest(V, M, fv, fm, key):
    """not kissing: her head turned, her cheek resting on his upper chest (on his skin); his head turned over hers,
    his chin kept off her head"""
    V.look(V.rig.head("頭") + left_of(fv) * 1.0 + fv * 0.15 - UP * 0.15, neck_share=0.45)   # face turned to her left: right cheek to him
    M.look(V.rig.head("頭") + left_of(fm) * 0.4 + fm * 0.6 - UP * 0.05, neck_share=0.5)    # he looks past her head
    near = (M.rig.head("首") * 0.55 + M.lm("chest.L") * 0.225 + M.lm("chest.R") * 0.225) - fm * 0.10
    loc, n, _, _ = surface(M).find_nearest(near)
    target = Vector(loc) + Vector(n) * 0.006
    def goal():
        c = V.lm("cheek.R" if (V.lm("cheek.R") - V.lm("cheek.L")).dot(fm) < 0 else "cheek.L")
        chin = (M.lm("mouth") - skull(V)).length
        return max(0.0, (c - target).length - 0.008) + 3.0 * max(0.0, 0.16 - chin)
    return solve_heads(V, M, fv, fm, goal, key, roll=(0.0, 4.0), weight=(1.0, 3.0), yaw_r=(0.0, 0.0), clear=0.19)

def chest_gap(V, M):
    """how far her chest front is from his skin along the line between them (m; - = pressing in)"""
    bvh = surface(M); g = 1.0
    for s in (".L", ".R"):
        p = V.lm("nipple" + s); loc, n, _, _ = bvh.find_nearest(p)
        if loc is not None: g = min(g, (p - Vector(loc)).dot(Vector(n)))
    return g

def embrace_arms(V, M, fv, fm, kiss):
    """hers round his neck (palms on the back of his neck / upper back), his round her (palms on her back: one at
    the waist, one higher); each hand on the partner's side it is nearest to"""
    bvhV = surface(V); bvhM = surface(M); r = V.rig; m = M.rig
    lv, lm = left_of(fv), left_of(fm)
    for s, sx in arm_side(V, fv):
        side = lv * sx; sh = r.head("腕" + s)
        T = M.lm("nape") + side * 0.045 - UP * (0.035 if kiss else 0.06)
        cup(V, s, bvhM, T, nrm(-UP * 0.4 - side * 0.6 + fv * 0.2), sh + side * 0.35 - UP * 0.25 - fv * 0.05)
    for s, sx in arm_side(M, fm):
        side = lm * sx; sh = m.head("腕" + s)
        her = ".L" if (V.lm("back_low.L") - V.lm("back_low.R")).dot(side) > 0 else ".R"
        T = V.lm(("back_low" if sx > 0 else "back_mid") + her) - UP * 0.01
        cup(M, s, bvhV, T, nrm(-side * 0.8 - UP * 0.25), sh + side * 0.30 - UP * 0.25 + fm * 0.18)   # elbow by her flank: the arm wraps round

def embrace(ctx, t, kiss):
    V, M = ctx.V, ctx.M
    ph = 2 * math.pi * t; breath = math.sin(ph)
    C = V3(0.0, -0.55, 0.0); ax = nrm(V3(0.84, -0.17, 0))              # her -> him, where they stood talking
    key = "kiss.stand" if kiss else "stand.embrace"
    leanV, leanM, rounds = (3.0, 10.0, 6) if kiss else (-3.0, 6.0, 1)
    if key in _LAYOUT: leanV, leanM = _LAYOUT[key]; rounds = 1          # decided on the first frame: no pops
    d = 0.32
    for rnd in range(rounds):
        for it in range(5):                                              # bodies close until her chest just meets his
            fv, lv = stand(V, (C - ax * d * 0.48).xy, ax, weight=0.55 + 0.08 * breath, breath=breath, chest_lean=leanV, feet="stagger")
            fm, lm = stand(M, (C + ax * d * 0.52).xy, -ax, weight=-0.3 + 0.06 * math.sin(ph + 1.1), breath=math.sin(ph + 0.6),
                           chest_lean=leanM, feet="wide")
            SC.apply_rigs(V, M)
            g = chest_gap(V, M)
            if abs(g + 0.006) < 0.003: break
            d = max(0.10, d - (g + 0.006))                               # breasts press ~6 mm into the embrace
        if not kiss: break
        if kiss_heads(V, M, fv, fm, "kiss.stand") < 0.003 or rnd == rounds - 1: break
        leanV += 3.0; leanM += 4.0                                       # the necks ran out of range: lean in from the hips
    _LAYOUT[key] = (leanV, leanM)
    embrace_arms(V, M, fv, fm, kiss)
    if kiss: kiss_heads(V, M, fv, fm, "kiss.stand", nod=1.2 * math.sin(ph))   # her hands are on his neck now: settle again
    else: cheek_on_chest(V, M, fv, fm, "stand.embrace")

# ---------------------------------------------------------------- sitting on the bed's edge
def sit(body, x, y, face, lean=8.0, chest_turn=0.0, knee_out=0.05, breath=0.0, side_lean=0.0):
    """sitting on the right edge of the bed (facing ~+X): buttocks on the mattress, thighs over the edge, shins
    hanging free of the frame (the bed is 60 cm high: feet do not reach the floor), feet relaxed"""
    r = body.rig; r.reset()
    f = flat(face); left = left_of(f)
    a = math.radians(lean + 1.0 * breath)
    body.place(V3(x, y, BED_TOP + 0.12), f, UP)
    cf = flat(yaw(f, chest_turn))
    sl = math.radians(side_lean)                                      # + = toward the body's left (a partner beside)
    up_s = nrm(UP * math.cos(sl * 0.4) + left * math.sin(sl * 0.4)); up_c = nrm(UP * math.cos(sl) + left * math.sin(sl))
    body.spine(forward=nrm(f * math.cos(a) - up_s * math.sin(a)), up=nrm(f * math.sin(a) + up_s * math.cos(a)),
               chest_forward=nrm(cf * math.cos(a * 0.5) - up_c * math.sin(a * 0.5)), chest_up=nrm(cf * math.sin(a * 0.5) + up_c * math.cos(a * 0.5)))
    SC.settle(body, target_z=BED_TOP, sink=0.012, region=_mask(body, "seat"))
    for s, sx in lr(body, f):
        H = r.head("足" + s); L1 = body.len["thigh" + s]
        K = H + f * (L1 * 0.97) + left * sx * knee_out - UP * 0.02
        body.leg_aim(s, K, nrm(-UP + f * 0.20 + left * sx * 0.04))
        body.foot_relaxed(s, 24, splay=left * sx)
    return f, left

def hands_on_thighs(body, f, left, which=None):
    r = body.rig; bvh = surface(body)
    for s, sx in arm_side(body, f):
        if which is not None and s not in which: continue
        leg = [ls for ls, lx in lr(body, f) if lx == sx][0]
        H = r.head("足" + leg); K = r.head("ひざ" + leg)
        cup(body, s, bvh, H + (K - H) * 0.62 + UP * 0.07, nrm(K - H), r.head("腕" + s) + left * sx * 0.12 - UP * 0.3 - f * 0.05)

def sit_bed(ctx, t):
    V, M = ctx.V, ctx.M; r = V.rig; m = M.rig
    ph = 2 * math.pi * t
    fv, lv = sit(V, MAT_W / 2 - 0.20, 0.95, V3(1, 0, 0), lean=6.0, breath=math.sin(ph))
    fm, lm = sit(M, MAT_W / 2 - 0.22, 1.45, V3(1, 0, 0), lean=10.0, breath=math.sin(ph + 0.8))
    hands_on_thighs(V, fv, lv)
    bvhV = surface(V)
    near = [s for s, sx in arm_side(M, fm) if (m.head("腕" + s) - r.head("上半身2")).length ==
            min((m.head("腕" + q) - r.head("上半身2")).length for q in (".L", ".R"))][0]
    far = ".R" if near == ".L" else ".L"
    toward_her = nrm(r.head("上半身2") - m.head("上半身2")); toward_her.z = 0; toward_her.normalize()
    her_far = max((".L", ".R"), key=lambda q: (r.head("腕" + q) - r.head("上半身2")).dot(toward_her))   # his arm round her shoulders
    cup(M, near, bvhV, r.head("腕" + her_far) + UP * 0.07 - fv * 0.02, toward_her, m.head("腕" + near) - toward_her * 0.1 - UP * 0.25 - fm * 0.15)
    hands_on_thighs(M, fm, lm, which=(far,))
    V.look(m.head("頭") + V3(0.05, 0, -0.05)); M.look(r.head("頭"))

def kiss_sit(ctx, t):
    """seated side by side on the edge, turned to each other (the chest turns more than the hips), lips together;
    her near hand on his chest, his near hand at her waist, the far hands on their own thighs"""
    V, M = ctx.V, ctx.M; r = V.rig; m = M.rig
    ph = 2 * math.pi * t
    gap_y = 0.44                                                        # hips apart: thighs and feet stay clear of each other
    slv, slm, rounds = 8.0, 10.0, 6
    if "kiss.sit" in _LAYOUT: slv, slm = _LAYOUT["kiss.sit"]; rounds = 1
    for it in range(rounds):
        fv, lv = sit(V, MAT_W / 2 - 0.22, 1.20 - gap_y * 0.5, V3(1, -0.10, 0), lean=4.0, chest_turn=30.0, knee_out=-0.03,
                     breath=math.sin(ph), side_lean=slv)
        fm, lm = sit(M, MAT_W / 2 - 0.22, 1.20 + gap_y * 0.5, V3(1, 0.04, 0), lean=10.0, chest_turn=-28.0, knee_out=0.03,
                     breath=math.sin(ph + 0.8), side_lean=-slm)
        g = kiss_heads(V, M, fv, fm, "kiss.sit")
        if g < 0.003 or it == rounds - 1: break
        slv += 4.0; slm += 4.0                                          # lean in from the waist a little more
    _LAYOUT["kiss.sit"] = (slv, slm)
    bvhV = surface(V); bvhM = surface(M)
    vnear = [s for s, sx in arm_side(V, fv) if sx > 0][0]              # her left faces him (he sits at +y)
    vfar = ".R" if vnear == ".L" else ".L"
    mnear = [s for s, sx in arm_side(M, fm) if sx < 0][0]              # his right faces her
    mfar = ".R" if mnear == ".L" else ".L"
    chest = ".L" if (M.lm("chest.L") - r.head("上半身2")).length < (M.lm("chest.R") - r.head("上半身2")).length else ".R"
    cup(V, vnear, bvhM, M.lm("chest" + chest) + V3(0, 0, 0.03), nrm(UP + fv * 0.2), r.head("腕" + vnear) + V3(0.05, -0.15, -0.3))
    her_side = ".L" if (V.lm("back_low.L") - m.head("上半身2")).length < (V.lm("back_low.R") - m.head("上半身2")).length else ".R"
    cup(M, mnear, bvhV, V.lm("back_low" + her_side) + V3(0.03, 0, 0.03), nrm(V3(0.4, -1, -0.3)), m.head("腕" + mnear) + V3(-0.2, 0.1, -0.3))
    hands_on_thighs(V, fv, lv, which=(vfar,)); hands_on_thighs(M, fm, lm, which=(mfar,))
    kiss_heads(V, M, fv, fm, "kiss.sit", nod=1.2 * math.sin(ph))      # hands placed: the heads settle again

P.CLIPS.update({
    "stand.idle":    (lambda c, t: stand_idle(c, t), 49, "loop"),
    "stand.embrace": (lambda c, t: embrace(c, t, False), 49, "loop"),
    "kiss.stand":    (lambda c, t: embrace(c, t, True), 49, "loop"),
    "sit.bed":       (lambda c, t: sit_bed(c, t), 49, "loop"),
    "kiss.sit":      (lambda c, t: kiss_sit(c, t), 49, "loop"),
})
