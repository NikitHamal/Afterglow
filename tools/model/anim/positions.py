# Clip definitions. Each pose function poses BOTH rigs for normalized time t (depth / phase) in the shared
# bed world (Blender Z-up: x = across the bed, +y = toward the headboard, z = up).
import math
from mathutils import Vector, Matrix, Quaternion
from scene import BED_TOP, BED_C, HEAD_Y, FOOT_Y, MAT_W
import scene as SC

def V3(*a): return Vector(a)
def sides(body, bone="足"):
    """(".L"/".R", world x-sign of that limb): a body facing up has its left at +x, facing down or toward the
    headboard at -x — placement offsets must follow the real side, never the label"""
    r = body.rig; c = r.head("下半身").x
    return [(s, 1.0 if r.head(bone + s).x >= c else -1.0) for s in (".L", ".R")]
def nrm(v): v = Vector(v); return v.normalized()
def ss(e0, e1, x):
    t = max(0.0, min(1.0, (x - e0) / (e1 - e0))); return t * t * (3 - 2 * t)
def yz(angle_deg):
    a = math.radians(angle_deg); return V3(0, math.cos(a), math.sin(a))

SCORE = None            # author.py: () -> penalty of the current pose (bad contacts, depth, floors)
CURRENT = None          # clip being baked: best_of caches its choice per clip
_BEST = {}
def best_of(key, variants, apply, bodies):
    """pose with each variant (apply(v)), keep the one with the lowest SCORE. Decided once per clip (author.py
    runs every clip at mid-stroke first) so the choice never changes between frames."""
    k = (CURRENT, key)
    if k not in _BEST:
        if SCORE is None: _BEST[k] = 0
        else:
            snaps = [b.rig.snapshot() for b in bodies]; best = None
            for i, v in enumerate(variants):
                for b, sn in zip(bodies, snaps): b.rig.restore(sn)
                apply(v); sc = SCORE()
                if best is None or sc < best[0]: best = (sc, i)
            for b, sn in zip(bodies, snaps): b.rig.restore(sn)
            _BEST[k] = best[1]
            print("[anim]   best_of", CURRENT, key, variants[best[1]], round(best[0], 1), flush=True)
    apply(variants[_BEST[k]]); return variants[_BEST[k]]

LINS = 0.110            # insertion at depth 1 (m); her canal is opened to his girth by vag_open
TIP_OUT = 0.006         # at depth 0 the tip rests 6 mm outside the entrance

class Ctx:
    """shared measured anatomy: her canal (entrance + axis on 下半身), his shaft (竿根 -> 竿端)"""
    def __init__(self, V, M, viv_meas, mar_meas):
        self.V, self.M = V, M
        vg = viv_meas["vagina"]
        e = V3(*vg["entrance"]); e.x = 0.0; ax = V3(*vg["axis"]); ax.x = 0.0; ax.normalize()   # sagittal: PCA tilt removed
        V.add_landmark("vag_E", "下半身", e); V.lm_dir("vag_D", "下半身", ax)
        if "anus" in viv_meas:
            an = viv_meas["anus"]; V.add_landmark("anus_E", "下半身", V3(*an["entrance"])); V.lm_dir("anus_D", "下半身", V3(*an["axis"]))
        self.L_erect = mar_meas["erect_length"]
        # his natural erect direction: 20 deg above his pelvis' forward axis, in his sagittal plane
        r = M.rig
        nat_world = (V3(0, -1, 0) * math.cos(math.radians(20)) + V3(0, 0, 1) * math.sin(math.radians(20))).normalized()
        M.lm_dir("shaft_nat", "下半身", nat_world)

    def entrance(self, which="vag"):
        return self.V.lm(which + "_E"), self.V.dir(which + "_D")

    def penetrate(self, depth, which="vag"):
        """aim his shaft along her canal and slide him so the tip is `depth` inside (pelvis translated)"""
        M = self.M; r = M.rig
        E, D = self.entrance(which)
        r.aim("竿根", D)
        tip_in = depth * LINS - TIP_OUT
        B = E - D * (self.L_erect - tip_in)                # where the shaft root must be
        M.move(B - r.head("竿根"))
        return E, D

def align_pelvis_to(M, D, up_hint):
    """orient his pelvis so its natural erect direction == D, keeping his up close to up_hint"""
    r = M.rig
    # natural dir is fixed in pelvis-local: rotate the pelvis frame by the rotation that takes nat -> D,
    # then remove roll so the pelvis left axis stays horizontal-ish (perpendicular to D and up_hint)
    nat = M.dir("shaft_nat")
    q = nat.rotation_difference(D)
    R_new = q.to_matrix() @ r.P["下半身"].to_3x3().normalized()
    up = (R_new @ r.loc_up["下半身"]).normalized()
    # twist around D so his up is as close to up_hint as possible
    u_proj = up - D * up.dot(D); h_proj = V3(*up_hint) - D * V3(*up_hint).dot(D)
    if u_proj.length > 1e-6 and h_proj.length > 1e-6:
        ang = u_proj.normalized().angle(h_proj.normalized())
        if u_proj.normalized().cross(h_proj.normalized()).dot(D) < 0: ang = -ang
        R_new = Matrix.Rotation(ang, 3, D) @ R_new
    f = (R_new @ r.loc_fwd["下半身"]).normalized(); u = (R_new @ r.loc_up["下半身"]).normalized()
    M.place(r.head("下半身"), f, u)

# ---------------------------------------------------------------- shared limb recipes
def supine_her(V, pelvis_y, tilt=-15.0, knees_w=0.40, feet_w=0.40, feet_back=0.40, head_target=None):
    r = V.rig; r.reset()
    f = nrm(yz(90 + tilt)); u = nrm(yz(tilt))                     # front up (tilted toward the feet), head toward +y
    V.place(V3(0, pelvis_y, BED_TOP + 0.10), f, u)
    V.spine(forward=V3(0, 0.05, 1), up=V3(0, 1, -0.05), chest_forward=V3(0, 0.12, 1), chest_up=V3(0, 1, -0.12))
    SC.settle(V, sink=0.008)                                     # back + pelvis on the sheet, before the feet are planted
    for s, sx in sides(V, "足"):
        H = r.head("足" + s)
        A = V3(sx * feet_w, pelvis_y - feet_back, BED_TOP + 0.075)
        V.leg_ik(s, A, H + V3(sx * knees_w, -0.05, 0.55))
        V.foot(s, nrm(V3(sx * 0.25, -1.0, -0.35)), V3(sx * 0.1, -0.25, 1))
    if head_target is not None: V.look(head_target)

def her_arms_overhead(V):
    r = V.rig
    for s, sx in sides(V, "腕"):
        sh = r.head("腕" + s)
        W = V3(sx * 0.27, sh.y + 0.24, BED_TOP + 0.10)
        V.arm_ik(s, W, sh + V3(sx * 0.45, 0.05, -0.1))
        V.hand(s, palm=V3(-sx * 0.3, -0.2, 1), fingers=nrm(V3(sx * 0.15, 1, 0.1)))
        V.fingers(s, 0.45)

def her_rub_hand(V, s, phase, target, radius=0.011):
    """her hand on a point (clit): fingertip pads circle on the skin; returns the fingertip target"""
    r = V.rig; sx = 1 if s == ".L" else -1
    a = 2 * math.pi * phase
    # circle in the plane of the body surface (approximate: perpendicular to her front)
    fwd = V.fwd_dir("下半身"); up = V.up_dir("下半身"); left = V.left_dir("下半身")
    tip = target + (left * math.cos(a) + up * math.sin(a)) * radius
    # the wrist sits above the mons, hand pointing down toward the vulva, palm on the skin
    fingers_dir = (-up * 0.9 + fwd * -0.2).normalized()
    palm = -fwd
    W = tip - fingers_dir * 0.085 + fwd * 0.028
    sh = r.head("腕" + s)
    V.arm_ik(s, W, sh + left * sx * 0.12 + fwd * 0.12 - up * 0.35)       # elbow tucked to her side
    V.hand(s, palm=palm, fingers=fingers_dir)
    V.fingers(s, 0.25, index=0.12, thumb=0.2)
    return tip

def her_breast_hand(V, s, phase, nipple_lm):
    r = V.rig; sx = 1 if s == ".L" else -1
    N = V.lm(nipple_lm); fwd = V.fwd_dir("上半身2"); up = V.up_dir("上半身2"); left = V.left_dir("上半身2")
    k = 0.5 + 0.5 * math.sin(2 * math.pi * phase)
    W = N - up * 0.075 - left * sx * 0.03 + fwd * (0.035 + 0.008 * k)
    sh = r.head("腕" + s)
    V.arm_ik(s, W, sh + left * sx * 0.3 - fwd * 0.1 - up * 0.2)
    V.hand(s, palm=-fwd, fingers=(up * 0.9 + left * -sx * 0.4).normalized())
    V.fingers(s, 0.35 + 0.25 * k, thumb=0.4)

def kneel_his_legs(M, knee_w=0.17, knee_z=0.055, shin_dir=V3(0, -1, 0.04)):
    r = M.rig
    for s, sx in sides(M, "足"):
        H = r.head("足" + s); L = M.len["thigh" + s]
        dz = H.z - (BED_TOP + knee_z)
        dy = math.sqrt(max(1e-4, L * L - dz * dz - (sx * knee_w - H.x) ** 2))
        K = V3(sx * knee_w, H.y - dy, BED_TOP + knee_z)
        M.leg_aim(s, K, shin_dir)
        M.foot(s, nrm(V3(0, -1, -0.15)), V3(0, 0.1, -1))

def lean_to_reach(M, targets, pelvis_up_deg, angles=range(0, 75, 3), bend=0.9):
    """choose the chest pitch (degrees above horizontal, facing +y/down) so both shoulders reach their hand
    targets with a natural elbow bend (shoulder->target = bend * arm length)"""
    best = None
    for a in angles:
        M.spine(forward=nrm(yz(-90 + (a + pelvis_up_deg) / 2)), up=nrm(yz((a + pelvis_up_deg) / 2)),
                chest_forward=nrm(yz(-90 + a)), chest_up=nrm(yz(a)))
        err = 0.0
        for s, T in targets.items():
            L = M.len["upper" + s] + M.len["fore" + s]
            err += abs((M.rig.head("腕" + s) - T).length - bend * L)
        if best is None or err < best[0]: best = (err, a)
    a = best[1]
    M.spine(forward=nrm(yz(-90 + (a + pelvis_up_deg) / 2)), up=nrm(yz((a + pelvis_up_deg) / 2)),
            chest_forward=nrm(yz(-90 + a)), chest_up=nrm(yz(a)))
    return a

def his_hands_on_bed(M, y, w=0.31):
    r = M.rig
    for s, sx in sides(M, "腕"):
        sh = r.head("腕" + s)
        W = V3(sx * w, y, BED_TOP + 0.035)
        M.arm_ik(s, W, sh + V3(sx * 0.6, -0.25, 0.35))
        M.hand(s, palm=V3(0, 0, -1), fingers=nrm(V3(sx * 0.25, 1, 0)))
        M.fingers(s, 0.15)

# ---------------------------------------------------------------- MISSIONARY
def missionary(ctx, t, rub=None, embrace=0.0):
    V, M = ctx.V, ctx.M
    head_pt = V3(0, 1.62, BED_TOP + 0.55)
    supine_her(V, pelvis_y=0.98 + 0.012 * t, tilt=-24, knees_w=0.95, feet_w=0.62, feet_back=0.30, head_target=head_pt)
    SC.settle(V, sink=0.008)
    her_arms_overhead(V)
    if rub is not None:
        her_rub_hand(V, ".R", rub, V.lm("clit"))
    E, D = ctx.entrance()
    m = M.rig; m.reset()
    M.place(E - D * 0.25 + V3(0, 0, 0.05), V3(0, 1, -0.6), V3(0, 0.6, 1))
    align_pelvis_to(M, D, up_hint=V3(0, 0.75, 0.66))
    ctx.penetrate(0.5)                                                 # lean is solved at mid-stroke, then kept
    sh_her = (V.rig.head("腕.L") + V.rig.head("腕.R")) / 2
    hy = sh_her.y + 0.06                                               # hands beside her shoulders, forearms clear of her chest
    pel_up = math.degrees(math.atan2(M.up_dir("下半身").z, M.up_dir("下半身").y))
    if embrace <= 0:
        targets = {".L": V3(0.50, hy, BED_TOP + 0.035), ".R": V3(-0.50, hy, BED_TOP + 0.035)}
        lean_to_reach(M, targets, pel_up)
    else:                                                              # down on his forearms, planted wide of her shoulders
        targets = {".L": V3(0.47, sh_her.y + 0.22, BED_TOP + 0.05), ".R": V3(-0.47, sh_her.y + 0.22, BED_TOP + 0.05)}
        lean_to_reach(M, targets, pel_up, bend=0.62)
    ctx.penetrate(t)
    kneel_his_legs(M, knee_w=0.13)
    if embrace <= 0:
        for s, sx in sides(M, "腕"):
            sh = M.rig.head("腕" + s)
            M.arm_ik(s, V3(sx * 0.50, hy, BED_TOP + 0.065), sh + V3(sx * 0.9, -0.15, 0.2))
            M.hand(s, palm=V3(0, 0, -1), fingers=nrm(V3(sx * 0.25, 1, 0))); M.fingers(s, 0.15)
    else:
        hh = V.rig.head("頭")
        for s, sx in sides(M, "腕"):
            sh = M.rig.head("腕" + s)
            M.arm_ik(s, V3(sx * 0.47, sh_her.y + 0.22, BED_TOP + 0.05), sh + V3(sx * 0.9, -0.1, -0.3))
            M.hand(s, palm=V3(0, 0, -1), fingers=nrm(V3(-sx * 0.2, 1, 0))); M.fingers(s, 0.35)
        his_side = dict(sides(M, "腕"))
        def her_arms(v):
            kind, a, b = v
            for s, sx in sides(V, "腕"):
                sh = V.rig.head("腕" + s)
                hs = ".L" if his_side[".L"] == sx else ".R"            # his limb on the same world side
                if kind == "back":                                  # around his flanks, hands on his back
                    T = M.lm("back" + s) - M.up_dir("上半身2") * a + V3(sx * 0.04, 0, 0.035)
                    V.arm_ik(s, T, sh + V3(sx * 0.75, -0.05, b))
                    V.hand(s, palm=V3(0, 0, -1), fingers=nrm(V3(-sx * 0.6, 0.4, 0)))
                else:                                               # holding his upper arms from below
                    A0 = M.rig.head("腕" + hs); A1 = M.rig.head("ひじ" + hs)
                    T = A0 + (A1 - A0) * a + V3(0, 0, -0.055)
                    V.arm_ik(s, T, sh + V3(sx * 0.5, -0.1, b))
                    V.hand(s, palm=V3(0, 0, 1), fingers=nrm(V3(sx * 0.3, 0.6, 0.2)))
                V.fingers(s, 0.45)
        best_of("her_arms", [("back", d, z) for d in (0.0, 0.1) for z in (0.0, 0.5)] +
                [("arm", f, z) for f in (0.5, 0.6, 0.7, 0.8) for z in (-0.4, 0.0, 0.3)], her_arms, (V,))
    M.look(V.rig.head("頭") + V3(0, 0, 0.05))

# ---------------------------------------------------------------- SOLO (Vivian alone; Marcus watches from the bedside)
def marcus_bedside(M, look_at):
    m = M.rig; m.reset()
    M.place(V3(MAT_W / 2 + 0.55, 0.75, 1.0), V3(-1, 0.2, 0), V3(0, 0, 1))
    import numpy as _np
    SC.settle(M, target_z=0.0, sink=0.0, region=_np.ones(len(M.obj.data.vertices), bool))
    M.look(look_at)

def solo(ctx, phase, rub=True):
    V, M = ctx.V, ctx.M
    supine_her(V, pelvis_y=1.02, tilt=-10, knees_w=0.48, feet_w=0.30, feet_back=0.36, head_target=V3(0.4, 1.3, BED_TOP + 0.9))
    SC.settle(V, sink=0.008)
    if rub:
        her_rub_hand(V, ".R", phase, V.lm("clit"))
        her_breast_hand(V, ".L", phase, "nipple.L")
        SC.settle(V, sink=0.008)                                   # shoulder blades follow the arms down
    else:
        her_arms_overhead(V)
        b = 0.5 + 0.5 * math.sin(2 * math.pi * phase)              # idle: slow breath-like sway of the knees
        for s, sx in sides(V, "足"):
            H = V.rig.head("足" + s)
            V.leg_ik(s, V3(sx * 0.30, 1.02 - 0.36, BED_TOP + 0.075), H + V3(sx * (0.48 + 0.04 * b), -0.05, 0.55))
    marcus_bedside(M, V.rig.head("下半身"))

CLIPS = {
    # name: (fn, frames, kind)  kind: "depth" (t = depth 0..1) | "loop" (t = phase, loops) | "pose" (single pose)
    "missionary.in":      (lambda c, t: missionary(c, t), 25, "depth"),
    "missionary.rub":     (lambda c, t: missionary(c, 0.5, rub=t), 25, "loop"),
    "missionary.embrace": (lambda c, t: missionary(c, 0.5, embrace=1.0), 2, "pose"),
    "solo.rub":           (lambda c, t: solo(c, t, rub=True), 25, "loop"),
    "solo.idle":          (lambda c, t: solo(c, t, rub=False), 49, "loop"),
}
LAYER_BONES = {
    "missionary.rub": {"vivian": "arm.R"},
    "missionary.embrace": {"vivian": "arms+head", "marcus": "upper"},
}
