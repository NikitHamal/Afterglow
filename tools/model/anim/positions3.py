# More acts (non-penetrative + afterglow): handjob, titjob, face-riding, cuddling, lying make-out.
# Registers into positions.CLIPS / LAYER_BONES on import. Same rules as the other clips: bodies settle on the
# mattress, hands are placed on the skin (cup / touch), joints stay inside their range, layouts are searched.
import math
from mathutils import Vector, Matrix
import positions as P
from positions import V3, nrm, yz, ss, sides, best_of, surface, cup, touch_tips, supine_her, her_arms_overhead
from positions2 import kneel, supine_him, hands_to, align_to
from scene import BED_TOP
import scene as SC

def shaft(M, lean=0.45):
    """his erection leaning toward his belly (the natural angle lying on his back); returns root, direction"""
    m = M.rig; S = nrm(V3(0, lean, 1.0 - lean * 0.25)); m.aim("竿根", S); return m.head("竿根"), S

def kneel_between(ctx, R0, torso_pitch, dist, knee_w=0.14):
    """she kneels between his thighs facing his head, hips `dist` behind his shaft root, torso pitched forward"""
    V = ctx.V; r = V.rig; r.reset()
    V.place(V3(0, R0.y - dist, BED_TOP + 0.36), V3(0, 1, 0), V3(0, 0, 1))
    a = math.radians(torso_pitch)
    V.spine(forward=nrm(V3(0, math.cos(a * 0.6), -math.sin(a * 0.6))), up=nrm(V3(0, math.sin(a * 0.6), math.cos(a * 0.6))),
            chest_forward=nrm(V3(0, math.cos(a), -math.sin(a))), chest_up=nrm(V3(0, math.sin(a), math.cos(a))))
    kneel(V, knee_w, -1, V3(0, -1, 0.04), knee_z=0.045)

# ---------------------------------------------------------------- HANDJOB: she kneels between his legs, her hand slides on him
def handjob(ctx, t):
    V, M = ctx.V, ctx.M
    supine_him(M, pelvis_y=1.10, feet_w=0.55, knees=0.10)
    R0, S = shaft(M, 0.40)
    kneel_between(ctx, R0, 12, 0.40)
    r = V.rig; m = M.rig
    bvhM = surface(M)
    k = 0.5 - 0.5 * math.cos(t * math.pi)                         # hand glides base -> just under the head
    his_leg = dict(sides(M, "足"))
    for s, sx in sides(V, "腕"):
        sh = r.head("腕" + s)
        if s == ".R":
            side = V3(-sx, 0, 0); around = S.cross(side).normalized()
            cup(V, s, bvhM, R0 + S * (0.035 + 0.085 * k) + side * 0.02, around, sh + V3(sx * 0.35, -0.2, -0.25))
        else:                                                      # other hand on his thigh
            hs = ".L" if his_leg[".L"] == sx else ".R"
            H = m.head("足" + hs); K = m.head("ひざ" + hs)
            cup(V, s, bvhM, H + (K - H) * 0.30 + V3(0, 0, 0.08), (K - H).normalized(), sh + V3(sx * 0.4, -0.2, -0.2))
    V.look(R0 + S * 0.08 + V3(0, 0.25 * math.sin(t * math.pi), 0.10))   # glances up at him as her hand rises
    for s, sx in sides(M, "腕"):
        sh = m.head("腕" + s)
        hands_to(M, s, V3(sx * 0.34, sh.y + 0.02, BED_TOP + 0.075), V3(0, 0, -1), nrm(V3(sx, 0.2, 0)), sh + V3(sx * 0.3, -0.3, 0.1), 0.08)
    M.look(r.head("頭"))

# ---------------------------------------------------------------- TITJOB: his shaft between her breasts, her hands press them together
def titjob(ctx, t):
    V, M = ctx.V, ctx.M
    supine_him(M, pelvis_y=1.10, feet_w=0.78, knees=0.04)
    R0, S = shaft(M, 0.30)
    r = V.rig; m = M.rig
    slide = 0.5 - 0.5 * math.cos(t * math.pi)
    def lean(v):
        pitch, dist = v
        kneel_between(ctx, R0, pitch, dist, knee_w=0.10)
        cleav = (V.lm("nipple.L") + V.lm("nipple.R")) * 0.5 - V.up_dir("上半身2") * 0.02 + V.fwd_dir("上半身2") * 0.015
        V.move(R0 + S * (0.07 + 0.05 * slide) - cleav + V3(0, 0, 0.0))            # cleavage on the shaft
        kneel(V, 0.10, -1, V3(0, -1, 0.04), knee_z=0.045)
    best_of("lean", [(p_, d_) for p_ in (35, 45, 55) for d_ in (0.30, 0.38)], lean, (V,))
    bvhV = surface(V)
    for s, sx in sides(V, "腕"):                                   # hands on the outer sides of her breasts, pressing in
        N = V.lm("nipple" + s); left = V.left_dir("上半身2"); up = V.up_dir("上半身2")
        sxl = 1.0 if (N - r.head("上半身2")).dot(left) > 0 else -1.0
        cup(V, s, bvhV, N + left * sxl * 0.06 - up * 0.01, nrm(-left * sxl * 0.4 + up * 0.9), r.head("腕" + s) + left * sxl * 0.4 - up * 0.2)
    V.look(m.head("頭"))
    for s, sx in sides(M, "腕"):
        sh = m.head("腕" + s)
        hands_to(M, s, V3(sx * 0.34, sh.y + 0.02, BED_TOP + 0.075), V3(0, 0, -1), nrm(V3(sx, 0.2, 0)), sh + V3(sx * 0.3, -0.3, 0.1), 0.08)
    M.look(r.head("頭"))

# ---------------------------------------------------------------- FACE-RIDING: she kneels astride his head, facing the headboard
def facesit(ctx, t):
    V, M = ctx.V, ctx.M
    supine_him(M, pelvis_y=0.80, feet_w=0.22, knees=0.04)
    m = M.rig; r = V.rig
    m.orient("頭", forward=V3(0, 0.15, 1), up=V3(0, 1, -0.15))   # chin up, mouth toward her
    grind = 0.012 * math.sin(2 * math.pi * t)
    mouth = M.lm("mouth")
    def seat(v):
        lean, w = v
        r.reset()
        V.place(mouth + V3(0, 0.03, 0.18), V3(0, 1, 0), V3(0, 0, 1))
        E, D = ctx.entrance()
        V.move(mouth + V3(0, 0.006 + grind, 0.012) - E)            # vulva on his mouth, rocking a little
        a = math.radians(lean)
        V.spine(forward=nrm(V3(0, math.cos(a * 0.5), -math.sin(a * 0.5))), up=nrm(V3(0, math.sin(a * 0.5), math.cos(a * 0.5))),
                chest_forward=nrm(V3(0, math.cos(a), -math.sin(a))), chest_up=nrm(V3(0, math.sin(a), math.cos(a))))
        kneel(V, w, +1, V3(0, -1, 0.04), knee_z=0.045, min_dy=0.04)
    best_of("seat", [(l_, w_) for l_ in (5, 15, 25) for w_ in (0.20, 0.25, 0.30)], seat, (V,))
    bvhV = surface(V)
    for s, sx in sides(V, "腕"):                                   # one hand in her hair... both on her thighs
        H = r.head("足" + s); K = r.head("ひざ" + s)
        cup(V, s, bvhV, H + (K - H) * 0.5 + V3(0, 0, 0.06), (K - H).normalized(), r.head("腕" + s) + V3(sx * 0.4, -0.1, -0.2))
    V.look(V3(0, 2.4, BED_TOP + 0.9 - 0.3 * math.sin(math.pi * t)))
    for s, sx in sides(M, "腕"):                                   # his hands flat on the sheet beside his head
        sh = m.head("腕" + s)
        hands_to(M, s, V3(sx * 0.42, sh.y + 0.10, BED_TOP + 0.085), V3(0, 0, -1), nrm(V3(sx * 0.3, 1, 0)), sh + V3(sx * 0.5, -0.2, 0.2), 0.05)

# ---------------------------------------------------------------- CUDDLE (afterglow): she lies on her side against him, head on his chest
def cuddle(ctx, t):
    V, M = ctx.V, ctx.M
    supine_him(M, pelvis_y=0.98, feet_w=0.20, knees=0.03)
    m = M.rig; r = V.rig
    breath = 0.004 * math.sin(2 * math.pi * t)
    m.rotate_world("上半身2", Matrix.Rotation(breath * 2.0, 3, "X"))
    def lie(v):
        dx, roll = v
        r.reset()
        a = math.radians(roll)                                       # on her left side, facing him (-X), rolled toward him
        f = nrm(V3(-math.cos(a), 0, -math.sin(a))); u = V3(0, 1, 0)
        V.place(V3(-dx, 0.98 + 0.02, BED_TOP + 0.20), f, u)
        V.spine(forward=nrm(V3(-math.cos(a), 0.05, -math.sin(a))), up=V3(0, 1, 0.02), chest_forward=nrm(V3(-math.cos(a * 1.3), 0.05, -math.sin(a * 1.3))), chest_up=V3(0, 1, 0.04))
        SC.settle(V, sink=0.010)
        lo = ".L" if r.head("足.L").z < r.head("足.R").z else ".R"; hi = ".R" if lo == ".L" else ".L"
        H = r.head("足" + lo); V.leg_ik(lo, V3(H.x - 0.05, H.y - 0.78, BED_TOP + 0.06), H + V3(-0.3, -0.2, 0.05)); V.foot_relaxed(lo, 30)
        H = r.head("足" + hi)                                          # top leg drapes over his thigh
        V.leg_ik(hi, V3(H.x + 0.22, H.y - 0.70, BED_TOP + 0.26), H + V3(0.2, -0.3, 0.45)); V.foot_relaxed(hi, 30)
        mh = m.head("上半身2") + V3(0.06, 0.12, 0.12)                    # her head rests on his chest/shoulder
        V.look(mh + V3(0.3, 0.6, 0.05))
    best_of("lie", [(d_, ro) for d_ in (0.24, 0.30) for ro in (25, 40)], lie, (V,))
    bvhM = surface(M)
    lo = ".L" if r.head("腕.L").z < r.head("腕.R").z else ".R"; hi = ".R" if lo == ".L" else ".L"
    sh = r.head("腕" + hi)
    cup(V, hi, bvhM, m.head("上半身2") + V3(0.0, 0.10, 0.14), nrm(V3(1, 0.2, 0)), sh + V3(0.1, -0.2, 0.3))     # her arm across his chest
    sh = r.head("腕" + lo)
    hands_to(V, lo, V3(sh.x - 0.05, sh.y + 0.22, BED_TOP + 0.08), V3(0, 0, -1), nrm(V3(0.2, 1, 0)), sh + V3(-0.25, 0.0, 0.15), 0.1)
    bvhV = surface(V)
    hs = dict(sides(M, "腕")); his_near = ".L" if hs[".L"] < 0 else ".R"; his_far = ".R" if his_near == ".L" else ".L"
    cup(M, his_near, bvhV, r.head("上半身2") + V3(-0.04, 0.06, 0.10), nrm(V3(0, 1, 0)), m.head("腕" + his_near) + V3(-0.2, 0.1, -0.3))   # his arm around her shoulders
    sh = m.head("腕" + his_far)
    hands_to(M, his_far, V3(sh.x + 0.05, sh.y + 0.18, BED_TOP + 0.075), V3(0, 0, -1), nrm(V3(0, 1, 0)), sh + V3(0.3, -0.2, 0.1), 0.08)
    M.look(r.head("頭"))

# ---------------------------------------------------------------- KISS (lying make-out): face to face on their sides, lips together
def kiss_side(ctx, t):
    V, M = ctx.V, ctx.M; r = V.rig; m = M.rig
    sway = 0.006 * math.sin(2 * math.pi * t)
    r.reset()
    V.place(V3(-0.10, 1.00, BED_TOP + 0.22), V3(1, 0, 0), V3(0, 1, 0))          # on her left side facing +X
    V.spine(forward=V3(1, 0.05, 0), up=V3(0, 1, 0.03), chest_forward=V3(1, 0.05, 0), chest_up=V3(0, 1, 0.05))
    SC.settle(V, sink=0.010)
    lo = ".L" if r.head("足.L").z < r.head("足.R").z else ".R"; hi = ".R" if lo == ".L" else ".L"
    H = r.head("足" + lo); V.leg_ik(lo, V3(H.x + 0.05, H.y - 0.78, BED_TOP + 0.06), H + V3(0.3, -0.2, 0.05)); V.foot_relaxed(lo, 30)
    m.reset()
    M.place(V3(0.30, 0.94, BED_TOP + 0.24), V3(-1, 0, 0), V3(0, 1, 0))          # on his right side facing her
    M.spine(forward=V3(-1, 0.05, 0), up=V3(0, 1, 0.03), chest_forward=V3(-1, 0.05, 0), chest_up=V3(0, 1, 0.05))
    SC.settle(M, sink=0.010)
    def heads(v):
        tilt_v, tilt_m = v
        V.look(V3(0.5, r.head("頭").y + 0.02, r.head("頭").z + tilt_v))
        M.look(V3(-0.5, m.head("頭").y + 0.02, m.head("頭").z - tilt_m))
        # bring him so the lips meet (his whole body slides, heads tilted the opposite ways)
        M.move(V.lm("mouth") + V3(0.012 + sway, 0.0, 0.0) - M.lm("mouth"))
        SC.settle(M, sink=0.010)
    best_of("heads", [(a_, b_) for a_ in (0.10, 0.20) for b_ in (0.10, 0.20)], heads, (V, M))
    mlo = ".L" if m.head("足.L").z < m.head("足.R").z else ".R"; mhi = ".R" if mlo == ".L" else ".L"
    H = m.head("足" + mlo); M.leg_ik(mlo, V3(H.x - 0.05, H.y - 0.80, BED_TOP + 0.065), H + V3(-0.3, -0.2, 0.05)); M.foot_relaxed(mlo, 30)
    H = r.head("足" + hi)                                                    # her top leg over his hip
    V.leg_ik(hi, V3(H.x + 0.42, H.y - 0.55, BED_TOP + 0.24), H + V3(0.3, -0.2, 0.30)); V.foot_relaxed(hi, 30)
    H = m.head("足" + mhi); M.leg_ik(mhi, V3(H.x - 0.10, H.y - 0.80, BED_TOP + 0.17), H + V3(-0.3, -0.2, 0.10)); M.foot_relaxed(mhi, 30)
    bvhM = surface(M); bvhV = surface(V)
    vlo = ".L" if r.head("腕.L").z < r.head("腕.R").z else ".R"; vhi = ".R" if vlo == ".L" else ".L"
    cup(V, vhi, bvhM, m.head("頭") + V3(0.02, -0.03, 0.06), nrm(V3(0, 1, 0.3)), r.head("腕" + vhi) + V3(0.1, -0.2, 0.3))   # her hand on his cheek/neck
    sh = r.head("腕" + vlo); hands_to(V, vlo, V3(sh.x + 0.12, sh.y + 0.22, BED_TOP + 0.08), V3(0, 0, -1), nrm(V3(0.3, 1, 0)), sh + V3(0.3, 0.0, 0.15), 0.1)
    plo = ".L" if m.head("腕.L").z < m.head("腕.R").z else ".R"; phi = ".R" if plo == ".L" else ".L"
    cup(M, phi, bvhV, r.head("上半身") + V3(-0.02, 0.02, 0.10), nrm(V3(0, -0.4, 1)), m.head("腕" + phi) + V3(-0.1, -0.2, 0.3))   # his hand on her waist/back
    sh = m.head("腕" + plo); hands_to(M, plo, V3(sh.x - 0.12, sh.y + 0.24, BED_TOP + 0.08), V3(0, 0, -1), nrm(V3(-0.3, 1, 0)), sh + V3(-0.3, 0.0, 0.15), 0.1)

P.CLIPS.update({
    "handjob.in":  (lambda c, t: handjob(c, t), 25, "depth"),
    "titjob.in":   (lambda c, t: titjob(c, t), 25, "depth"),
})
