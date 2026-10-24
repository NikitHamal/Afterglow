# Remaining positions: doggy, cowgirl / reverse cowgirl, legs-up, oral, lick, spoon, prone.
# Registers into positions.CLIPS / LAYER_BONES on import. Rule everywhere: place torso -> settle on core
# anatomy -> legs/arms to world targets (so feet/hands/knees are planted after the body found the mattress).
import math
from mathutils import Vector, Matrix
import positions as P
from positions import V3, nrm, yz, ss, LINS, TIP_OUT, sides, supine_her, her_arms_overhead, her_rub_hand, align_pelvis_to, best_of, surface, cup, touch_tips
from scene import BED_TOP, MAT_W
import scene as SC

def align_to(body, bone, dir_lm, D, up_hint):
    """rotate the whole body so a direction landmark riding `bone` points along D, rolled so the body's up ~ up_hint"""
    r = body.rig
    cur = body.dir(dir_lm); q = cur.rotation_difference(D)
    R_new = q.to_matrix() @ r.P[bone].to_3x3().normalized()
    up = (R_new @ r.loc_up[bone]).normalized()
    u_proj = up - D * up.dot(D); h = V3(*up_hint); h_proj = h - D * h.dot(D)
    if u_proj.length > 1e-6 and h_proj.length > 1e-6:
        un, hn = u_proj.normalized(), h_proj.normalized(); ang = un.angle(hn)
        if un.cross(hn).dot(D) < 0: ang = -ang
        R_new = Matrix.Rotation(ang, 3, D) @ R_new
    f = (R_new @ r.loc_fwd[bone]).normalized(); u = (R_new @ r.loc_up[bone]).normalized()
    r.place(body.root, bone, r.head(bone), f, u)

def kneel(body, knee_w, forward_of_hip, shin_dir, knee_z=0.05, foot_toes=None, foot_up=V3(0, 0, -1), min_dy=None, point_deg=78):
    """knees on the mattress at +-knee_w, `forward_of_hip` (+1 knees ahead of the hips along +y, -1 behind).
    returns how far (m) the knees float above their target when the hips are out of reach"""
    r = body.rig; gap = 0.0
    for s, sx in sides(body, "足"):
        H = r.head("足" + s); L = body.len["thigh" + s]
        dz = H.z - (BED_TOP + knee_z); dx = sx * knee_w - H.x
        if min_dy is not None:                      # hips risen out of reach: bring the knees in (never float)
            room = L * L - dz * dz - min_dy * min_dy
            if room < dx * dx: dx = math.copysign(math.sqrt(max(0.0, room)), dx)
        rest = L * L - dz * dz - dx * dx
        if rest < 0: gap = max(gap, math.sqrt(dz * dz + dx * dx) - L)
        dy = math.sqrt(max(1e-4, rest))
        K = V3(H.x + dx, H.y + forward_of_hip * dy, BED_TOP + knee_z)
        body.leg_aim(s, K, shin_dir if not callable(shin_dir) else shin_dir(sx))
        if foot_toes is None: body.foot_relaxed(s, point_deg)             # kneeling on the instep, toes back
        else: body.foot(s, foot_toes(sx) if callable(foot_toes) else foot_toes, foot_up)
    return gap

_HANDV = {}
def _hand_low(body, s):
    """lowest point (z) of the posed, skinned hand (palm + fingers) of side s"""
    import numpy as np
    key = (body.obj.name, s)
    if key not in _HANDV:
        _HANDV[key] = np.where(np.array([b.startswith(("手首", "親指", "人指", "中指", "薬指", "小指")) and b.endswith(s)
                                         for b in SC.dominant_bone(body.obj)]))[0]
    SC.apply_rigs(body)
    Pv, _ = SC.evaluated(body.obj)
    return float(Pv[_HANDV[key], 2].min())

def hands_to(body, s, target, palm, fingers, pole, curl=0.4, plant=False):
    """wrist to target, hand oriented, fingers curled. A hand near the sheet never sinks into it (the wrist range
    can tip the fingers down: the wrist is lifted until the skin clears); plant=True (a hand bearing weight): the
    palm presses ~4 mm into the soft mattress, never hovers"""
    W = V3(*target)
    for _ in range(3):
        body.arm_ik(s, W, pole); body.hand(s, palm=palm, fingers=fingers); body.rig.limit("手首" + s); body.fingers(s, curl)
        if W.z > BED_TOP + 0.15: break
        low = _hand_low(body, s)
        want = BED_TOP - 0.004
        if (plant and abs(low - want) < 0.002) or (not plant and low >= BED_TOP - 0.006): break
        W = W + V3(0, 0, want - low)

def supine_him(M, pelvis_y, feet_w=0.22, knees=0.12):
    r = M.rig; r.reset()
    M.place(V3(0, pelvis_y, BED_TOP + 0.10), V3(0, 0, 1), V3(0, 1, 0))
    M.spine(forward=V3(0, 0.02, 1), up=V3(0, 1, -0.02), chest_forward=V3(0, 0.15, 1), chest_up=V3(0, 1, -0.15))
    SC.settle(M, sink=0.012)
    for s, sx in sides(M, "足"):
        H = r.head("足" + s)
        A = V3(sx * feet_w, H.y - 0.82 + knees, BED_TOP + 0.10)
        M.leg_ik(s, A, H + V3(sx * 0.1, -0.3, 0.5))
        M.foot_relaxed(s, 22, splay=V3(sx, 0, 0))

def prone_legs(body, feet_w, back=0.85, knee_out=0.1):
    r = body.rig
    for s, sx in sides(body, "足"):
        H = r.head("足" + s)
        body.leg_ik(s, V3(sx * feet_w, H.y - back, BED_TOP + 0.075), H + V3(sx * knee_out, -0.4, -0.5))
        body.foot_relaxed(s, 72)                                 # toes resting on the sheet

# ---------------------------------------------------------------- DOGGY
def doggy(ctx, t, rub=None):
    V, M = ctx.V, ctx.M; r = V.rig; r.reset()
    k = ss(0.45, 1.0, t)
    V.place(V3(0, 1.02 + 0.006 * t + 0.010 * k, BED_TOP + 0.43), nrm(yz(-112 - 4 * k)), nrm(yz(-22 - 4 * k)))
    sh_target = V3(0, 1.02 + 0.58, BED_TOP + 0.30)
    chest_up = (sh_target - r.head("上半身2")).normalized()
    chest_fwd = chest_up.cross(V3(1, 0, 0)).normalized()
    if chest_fwd.z > 0: chest_fwd = -chest_fwd
    V.spine(forward=nrm(yz(-100)), up=nrm(yz(-10)), chest_forward=chest_fwd, chest_up=chest_up)
    kneel(V, 0.27, -1, lambda sx: nrm(V3(sx * 0.12, -1, 0.05)), knee_z=0.045)
    for s, sx in sides(V, "腕"):
        sh = r.head("腕" + s)
        if rub is not None and s == ".R":
            her_rub_hand(V, s, rub, V.lm("clit")); continue
        hands_to(V, s, V3(sx * 0.15, sh.y + 0.24, BED_TOP + 0.085), V3(0, 0, -1), nrm(V3(sx * 0.1, 1, -0.1)), sh + V3(sx * 0.2, -0.1, -0.6),
                 0.45 + 0.25 * k, plant=True)                       # her weight is on her hands
    V.look(V3(0.06 * k, r.head("頭").y + 0.55 + 0.15 * k, BED_TOP + 0.02 + 0.10 * k))   # the sheet ahead of her hands
    E, D = ctx.entrance()
    m = M.rig; m.reset()
    M.place(E - D * 0.25, V3(0, 1, -0.2), V3(0, 0.2, 1))
    align_pelvis_to(M, D, up_hint=V3(0, 0.25, 1))
    M.spine(forward=nrm(yz(-20)), up=nrm(yz(70)), chest_forward=nrm(yz(-28)), chest_up=nrm(yz(62)))
    ctx.penetrate(t)
    kneel(M, 0.12, -1, V3(0, -1, 0.04), knee_z=0.045)
    bvhV = surface(V); her_hip = dict(sides(V, "足"))
    for s, sx in sides(M, "腕"):
        hp = V.lm("hip" + (".L" if her_hip[".L"] == sx else ".R"))
        cup(M, s, bvhV, hp + V3(0, -0.02, 0.04), nrm(V3(-sx * 0.3, 0.6, -1)), m.head("腕" + s) + V3(sx * 0.5, -0.2, -0.1))
    M.look(r.head("上半身2"))

# ---------------------------------------------------------------- COWGIRL / REVERSE COWGIRL
COW_STROKE = 0.05
PRONE_LINS = 0.085      # prone: her buttocks limit the depth
def cowgirl(ctx, t, reverse=False, rub=None):
    V, M = ctx.V, ctx.M
    supine_him(M, pelvis_y=1.0, feet_w=0.10, knees=0.04)
    m = M.rig; r = V.rig
    face = V3(0, -1, 0) if reverse else V3(0, 1, 0)
    k = ss(0.45, 1.0, t)
    tip_in = t * COW_STROKE + (0.075 if reverse else 0.065) - TIP_OUT   # she sits down onto him; knees rest on the sheet
    def seat(v):
        """shaft tilt (+ toward his head: she sits over his belly and leans in, her thighs straddle his waist
        instead of his hips), her torso lean, knee width (+ shin fan in reverse)"""
        tilt, lean0, w = v[0], v[1], v[2]
        S = nrm(V3(0, tilt, 0.97)); m.aim("竿根", S); R0 = m.head("竿根")
        r.reset()
        V.place(R0 + V3(0, 0, 0.12), face, V3(0, 0, 1))
        align_to(V, "下半身", "vag_D", S, up_hint=V3(0, -0.2 if reverse else 0.2, 1))
        V.move(R0 + S * (ctx.L_erect - tip_in) - V.lm("vag_E"))
        lean = lean0 - 5 * k
        upv = nrm(V3(0, (-1 if reverse else 1) * math.sin(math.radians(lean)), math.cos(math.radians(lean))))
        V.spine(forward=nrm(face * 0.95 + V3(0, 0, -0.2)), up=upv, chest_forward=nrm(face + V3(0, 0, -0.35)), chest_up=nrm(upv + face * 0.25))
        if not reverse:                         # knees beside his waist/ribs, shins back along his sides
            kneel(V, w, +1, lambda sx: nrm(-face + V3(sx * 0.22, 0, 0.03)), knee_z=0.045, min_dy=0.04)
        else:                                   # knees ahead of her beside his thighs, shins fanned back past his hips
            kneel(V, w, -1, lambda sx: nrm(V3(sx * v[3], 1, 0.04)), knee_z=0.045, min_dy=0.04)
    if not reverse:
        best_of("seat", [(tl, ln, w) for tl, ln in ((-0.22, 18), (0.05, 26), (0.30, 36)) for w in (0.20, 0.23, 0.26, 0.29, 0.32, 0.35)], seat, (V, M))
    else:
        best_of("seat", [(tl, 18, w, f) for tl in (0.30, 0.50) for w in (0.24, 0.30, 0.36, 0.42) for f in (0.5, 0.9)], seat, (V, M))
    bvhM = surface(M); bvhV_self = None
    his_leg = dict(sides(M, "足"))
    for s, sx in sides(V, "腕"):
        if rub is not None and s == ".R":
            her_rub_hand(V, s, rub, V.lm("clit")); continue
        sh = r.head("腕" + s)
        if reverse:                                         # her hands on his thighs, each on her own side
            T = M.lm("thigh" + (".L" if his_leg[".L"] == sx else ".R")) + V3(0, 0, 0.05)
            cup(V, s, bvhM, T, nrm(V3(0, -1, -0.2)), sh + V3(sx * 0.4, 0.1, 0))
        else:
            T = M.lm("chest" + (".R" if s == ".L" else ".L")) + V3(0, 0, 0.035)
            cup(V, s, bvhM, T, nrm(V3(-sx * 0.2, 1, -0.2)), sh + V3(sx * 0.5, -0.2, 0))
    V.look((m.head("頭") + V3(0.08 * k, 0, 0.45 * k)) if not reverse else V3(0.2 * k, -1.5, 1.0 + 0.6 * k))
    bvhV = surface(V); her_hip = dict(sides(V, "足"))
    def rev_arms(pole):
        for s, sx in sides(M, "腕"):
            hp = V.lm("hip" + (".L" if her_hip[".L"] == sx else ".R"))
            cup(M, s, bvhV, hp + V3(0, 0.02, 0.03), nrm(V3(0, -0.3, 1)), m.head("腕" + s) + V3(sx * pole[0], pole[1], pole[2]))
    if reverse:
        def rev_arms_any(v):
            if v[0] == "hips": return rev_arms(v[1])
            for s, sx in sides(M, "腕"):                            # relaxed: hands on the sheet beside his head
                hh = m.head("頭"); sh = m.head("腕" + s)
                hands_to(M, s, V3(sx * v[1], hh.y + 0.02, BED_TOP + 0.035), V3(0, 0, -1), nrm(V3(sx * 0.3, 1, 0)),
                         sh + V3(sx * 0.6, -0.1, 0.1), 0.3)
        best_of("his_arms", [("hips", (0.3, 0.1, 0.5)), ("hips", (0.6, 0.0, 0.1)), ("hips", (0.5, 0.2, 0.3)), ("bed", 0.30), ("bed", 0.42)],
                rev_arms_any, (M,))
    for s, sx in sides(M, "腕"):
        if reverse:
            continue
        else:
            hp = V.lm("hip" + (".L" if her_hip[".L"] == sx else ".R"))
            cup(M, s, bvhV, hp + V3(0, 0, 0.02), nrm(V3(0, 0.3, 1)), m.head("腕" + s) + V3(sx * 0.4, -0.2, -0.3))
    M.look(r.head("頭"))

# ---------------------------------------------------------------- LEGS UP (her legs raised in a V, he holds her ankles)
def legsup(ctx, t):
    V, M = ctx.V, ctx.M; r = V.rig; r.reset()
    V.place(V3(0, 0.98 + 0.012 * t, BED_TOP + 0.13), nrm(yz(90 - 22)), nrm(yz(-22)))
    V.spine(forward=V3(0, 0.15, 1), up=V3(0, 1, -0.15), chest_forward=V3(0, 0.08, 1), chest_up=V3(0, 1, -0.08))
    SC.settle(V, sink=0.008)
    k = ss(0.45, 1.0, t)
    her_arms_overhead(V, grip=0.3 + 0.7 * k)
    V.look(V3(0.12 * k, 0.75 + 0.6 * k, BED_TOP + 0.95))
    E, D = ctx.entrance()
    m = M.rig; m.reset()
    M.place(E - D * 0.25, V3(0, 1, -0.3), V3(0, 0.3, 1))
    align_pelvis_to(M, D, up_hint=V3(0, 0.3, 0.95))
    M.spine(forward=nrm(yz(-8)), up=nrm(yz(82)), chest_forward=nrm(yz(-12)), chest_up=nrm(yz(78)))
    ctx.penetrate(t)
    kneel(M, 0.16, -1, V3(0, -1, 0.04), knee_z=0.05)
    her_leg = dict(sides(V, "足"))
    def legs_and_hands(v):
        """her legs up in a V (ankles wide of his head); his hands hold her legs from the outside, high on the calves
        (his forearms run along the outside of her shins, never across her knees), elbows out and down"""
        spread, up, along, pole = v
        for s, sx in sides(V, "足"):
            H = r.head("足" + s)
            V.leg_ik(s, m.head("頭") + V3(sx * spread, 0.04, up), H + V3(sx * 0.4, 0.2, 0.3))
            V.foot_relaxed(s, 28, splay=V3(sx, 0, 0))
        bvhV = surface(V)
        for s, sx in sides(M, "腕"):
            hs = ".L" if her_leg[".L"] == sx else ".R"
            K = r.head("ひざ" + hs); A = r.head("足首" + hs)
            shin = (A - K).normalized()
            out = V3(sx, 0, 0) - shin * shin.x * sx; out = out.normalized() if out.length > 1e-6 else V3(sx, 0, 0)
            cup(M, s, bvhV, K + (A - K) * along + out * 0.05, shin, m.head("腕" + s) + V3(sx * pole[0], pole[1], pole[2]))
    best_of("legs", [(sp, up, al, p) for sp in (0.44, 0.54, 0.64) for up in (0.16, 0.30) for al in (0.25, 0.45)
                     for p in ((0.6, -0.2, -0.35), (0.45, 0.1, -0.6))], legs_and_hands, (V, M))
    M.look(r.head("頭"))

# ---------------------------------------------------------------- ORAL (she kneels between his legs) / LICK
def oral(ctx, t):
    V, M = ctx.V, ctx.M
    supine_him(M, pelvis_y=1.20, feet_w=0.66, knees=0.08)
    m = M.rig
    S = nrm(V3(0, 0.28, 0.96)); m.aim("竿根", S)
    R0 = m.head("竿根")
    depth = t * 0.075 - 0.004
    mouth_target = R0 + S * (ctx.L_erect - depth)
    r = V.rig; best = None
    for lean in range(10, 80, 4):                                    # torso lean that brings her mouth to the tip
        r.reset()
        V.place(V3(0, R0.y - 0.50, BED_TOP + 0.42), nrm(V3(0, 1, -0.2)), nrm(V3(0, 0.2, 1)))
        V.spine(forward=nrm(yz(-lean)), up=nrm(yz(90 - lean)), chest_forward=nrm(yz(-lean - 12)), chest_up=nrm(yz(78 - lean)))
        face = -S; up = nrm(V3(0, 1, 0) - face * face.dot(V3(0, 1, 0)))
        r.orient("首", forward=nrm(face + V3(0, 0.4, 0)), up=up); r.orient("頭", forward=face, up=up)
        err = (V.lm("mouth") - mouth_target).length
        if best is None or err < best[0]: best = (err, lean)
    lean = best[1]
    r.reset()
    V.place(V3(0, R0.y - 0.50, BED_TOP + 0.42), nrm(V3(0, 1, -0.2)), nrm(V3(0, 0.2, 1)))
    V.spine(forward=nrm(yz(-lean)), up=nrm(yz(90 - lean)), chest_forward=nrm(yz(-lean - 12)), chest_up=nrm(yz(78 - lean)))
    face = -S; up = nrm(V3(0, 1, 0) - face * face.dot(V3(0, 1, 0)))
    r.orient("首", forward=nrm(face + V3(0, 0.4, 0)), up=up); r.orient("頭", forward=face, up=up)
    V.move(mouth_target - V.lm("mouth"))
    kneel(V, 0.13, -1, V3(0, -1, 0.04), knee_z=0.045)
    bvhM = surface(M)
    his_leg = dict(sides(M, "足"))
    for s, sx in sides(V, "腕"):
        sh = r.head("腕" + s)
        if s == ".R":                                   # grip low on the shaft, fingers around it
            side = V3(sx, 0, 0); around = S.cross(side).normalized()
            cup(V, s, bvhM, R0 + S * 0.06 + side * 0.03, around, sh + V3(sx * 0.3, -0.25, -0.2))
        else:                                           # the other hand on his inner thigh, near the groin
            hs = ".L" if his_leg[".L"] == sx else ".R"
            H = M.rig.head("足" + hs); K = M.rig.head("ひざ" + hs)
            cup(V, s, bvhM, H + (K - H) * 0.40 + V3(0, 0, 0.08), (K - H).normalized(), sh + V3(sx * 0.4, -0.2, -0.2))
    for s, sx in sides(M, "腕"):
        sh = m.head("腕" + s)
        hands_to(M, s, V3(sx * 0.36, sh.y + 0.05, BED_TOP + 0.075), V3(0, 0, -1), nrm(V3(sx, 0.2, 0)), sh + V3(sx * 0.3, -0.3, 0.1), 0.08)
    M.look(r.head("頭"))

def lick(ctx, t):
    V, M = ctx.V, ctx.M
    supine_her(V, pelvis_y=1.20, tilt=-12, knees_w=0.80, feet_w=0.50, feet_back=0.36, head_target=V3(0, 0.85, BED_TOP + 0.4),
               arch=0.35 * math.sin(t * math.pi))
    her_arms_overhead(V, grip=0.6, reach=0.12)
    r = V.rig; m = M.rig
    E, D = ctx.entrance()
    face = nrm(D * 0.6 + V3(0, 0.4, -0.2))
    up = nrm(V3(0, 0, 1) - face * face.dot(V3(0, 0, 1)))
    stroke = math.sin(t * math.pi) * 0.016
    def lie(pitch, drop=0.0):                                        # prone, chest raised `pitch` deg, head tipped down by `drop`
        m.reset()
        M.place(V3(0, E.y - 0.80, BED_TOP + 0.12), V3(0, 0, -1), V3(0, 1, 0))
        M.spine(forward=nrm(yz(-90 + pitch * 0.4)), up=nrm(yz(pitch * 0.4)), chest_forward=nrm(yz(-90 + pitch)), chest_up=nrm(yz(pitch)))
        SC.settle(M, sink=0.006)
        fc = nrm(face + V3(0, 0, -drop)); u_ = nrm(V3(0, 0, 1) - fc * fc.dot(V3(0, 0, 1)))
        m.orient("首", forward=nrm(fc + V3(0, 0.2, 0)), up=u_); m.orient("頭", forward=fc, up=u_)
        return E - D * 0.004 + V3(0, 0.012 - stroke, 0.006) - M.lm("mouth")
    if "lick_pitch" not in _TUCK:                                    # the pitch whose mouth height needs the least vertical move
        _TUCK["lick_pitch"] = min(((a, d) for a in range(0, 45, 5) for d in (0.0, 0.15, 0.3, 0.45)), key=lambda ad: abs(lie(*ad).z - 0.002))
        print("[anim]   lick pitch/drop", _TUCK["lick_pitch"], "dz", round(lie(*_TUCK["lick_pitch"]).z, 3), flush=True)
    delta = lie(*_TUCK["lick_pitch"])
    M.move(delta)                                                   # small reach; legs re-planted below
    kneel(M, 0.16, -1, nrm(V3(0, -0.35, 1)), knee_z=0.065, point_deg=38)
    tuck_shaft(M, "lick")
    her = dict(sides(V, "足"))
    bvhV = surface(V)
    def hands(v):                                                    # his hands hold her inner thighs open (same world side)
        f, pole = v
        for s, sx in sides(M, "腕"):
            sh = m.head("腕" + s)
            if f is None:                                            # or flat on the sheet beside his face
                hands_to(M, s, V3(sh.x + sx * 0.06, sh.y + 0.22, BED_TOP + 0.03), V3(0, 0, -1), nrm(V3(-sx * 0.2, 1, 0)),
                         sh + V3(sx * pole[0], pole[1], pole[2]), 0.25); continue
            hs = ".L" if her[".L"] == sx else ".R"
            H = r.head("足" + hs); K = r.head("ひざ" + hs); ax = (K - H).normalized()
            n_in = V3(-sx, 0, 0); n_in = (n_in - ax * n_in.dot(ax)).normalized()
            T = H + (K - H) * f + n_in * 0.095
            cup(M, s, bvhV, T, ax, sh + V3(sx * pole[0], pole[1], pole[2]))
    best_of("hands", [(f, p) for f in (0.7, 0.8, 0.9) for p in ((0.3, -0.3, -0.1), (0.0, -0.4, -0.2), (-0.2, -0.2, -0.3))], hands, (M,))
    hh = m.head("頭"); sh = r.head("腕.R"); sxr = dict(sides(V, "腕"))[".R"]      # her right hand rests in his hair
    pass                                                              # (both hands stay in the sheet)
    V.look(m.head("頭"))

_TUCK = {}
def tuck_shaft(M, key):
    """lying on his front: turn the (hidden) shaft so it stays above the mattress — searched once per clip in his
    pelvis frame (sagittal sweep), then reused so the bone never jitters"""
    import numpy as np
    m = M.rig; up = M.up_dir("下半身"); fwd = M.fwd_dir("下半身")
    if key not in _TUCK:
        SC.set_keys("marcus_", {"aroused": 0.0})
        mask = SC.vgroup_mask(M.obj, ("竿根", "竿中", "竿先", "玉袋"), 0.2)
        best = None
        for a in range(-180, 180, 20):
            c, s_ = math.cos(math.radians(a)), math.sin(math.radians(a))
            d = (up * c - fwd * s_).normalized()
            m.aim("竿根", d); SC.apply_rigs(M)
            P, _ = SC.evaluated(M.obj); z = float(P[mask, 2].min())
            if best is None or z > best[0]: best = (z, a)
        _TUCK[key] = best[1]
    a = math.radians(_TUCK[key])
    m.aim("竿根", (up * math.cos(a) - fwd * math.sin(a)).normalized())

# ---------------------------------------------------------------- SPOON / PRONE (her pelvis tilt solved numerically)
def solve_tilt(ctx, build_her, cond, tilts):
    best = None
    for tl in tilts:
        build_her(tl)
        E, D = ctx.entrance(); B = E - D * ctx.L_erect
        score = cond(E, D, B)
        if best is None or score < best[0]: best = (score, tl)
    build_her(best[1]); return best[1]

_TRUNK = {}
def trunk_region(body, legs=True):
    """settling region for lying on the side: pelvis, trunk, head (+ thighs) — never the arms, which get placed later"""
    import numpy as np
    k = (body.obj.name, legs)
    if k not in _TRUNK:
        keep = ("下半身", "上半身", "上半身2", "首", "頭", "乳親", "腰") + (("足.L", "足.R") if legs else ())
        _TRUNK[k] = np.array([b.startswith(keep) for b in SC.dominant_bone(body.obj)])
    return _TRUNK[k]

def side_legs(body, lo, hi, spec, pole_off, settle_first=False):
    """legs drawn up lying on the side: spec = ((kx, ky, kz, ay, az) bottom leg, (...) top leg) offsets from the hips.
    settle_first: the body settles on hip + side + shoulder (never the arms) after the legs moved; the bottom knee and
    the bottom foot (lying on its outer edge) are raised / lowered until each rests lightly on the sheet (6 / 4 mm)"""
    import numpy as np
    r = body.rig; lk = la = 0.0
    regions = {}
    for nm, bones in (("knee", ("ひざ" + lo,)), ("foot", ("足首" + lo, "つま先" + lo))):
        key = (body.obj.name, nm + lo)
        if key not in _TRUNK:
            _TRUNK[key] = np.array([b in bones for b in SC.dominant_bone(body.obj)])
        regions[nm] = _TRUNK[key]
    def pose():
        for s, (kx, ky, kz, ay, az) in ((lo, spec[0]), (hi, spec[1])):
            H = r.head("足" + s); dk, da = (lk, la) if s == lo else (0.0, 0.0)
            K = V3(H.x + kx, H.y + ky, BED_TOP + kz + dk)
            body.leg_ik(s, V3(K.x - 0.08, K.y + ay, BED_TOP + az + da), K + pole_off)
            body.foot(s, nrm(V3(0.45, -0.9, 0.0)), nrm(V3(1.0, 0.45, 0.0)))
    for rnd in range(4):
        pose()
        if settle_first and rnd < 3: SC.settle(body, sink=0.012, region=trunk_region(body, legs=True))
        SC.apply_rigs(body); Pv, _ = SC.evaluated(body.obj)
        ek = (BED_TOP - 0.006) - float(Pv[regions["knee"], 2].min())
        ef = (BED_TOP - 0.004) - float(Pv[regions["foot"], 2].min())
        lk += ek; la += ef - 0.5 * ek                                  # the ankle target moves with the knee's change
        if abs(ek) < 0.002 and abs(ef) < 0.002: break
    pose()

def spoon(ctx, t, rub=None):
    """both on their left sides, he behind her. She arches (pelvis tipped forward ~`arch`) so her canal opens
    toward him; his pelvis follows her canal; legs stacked and drawn up, bottom arms under the pillows."""
    V, M = ctx.V, ctx.M; r = V.rig; m = M.rig
    def her_body(arch):
        r.reset()
        a = math.radians(arch)
        V.place(V3(0.0, 1.0 + 0.01 * t, BED_TOP + 0.24), nrm(V3(math.cos(a), -math.sin(a), 0)), nrm(V3(math.sin(a), math.cos(a), 0)))
        V.spine(forward=V3(1, 0.12, 0), up=nrm(V3(-0.12, 1, 0.03)), chest_forward=V3(1, 0, 0), chest_up=nrm(V3(0, 1, 0.06)))
        SC.settle(V, sink=0.012, region=trunk_region(V))
    if "spoon_arch" not in _TUCK:                       # the arch whose canal is closest to his natural angle (lying behind her)
        nat = V3(math.cos(math.radians(20)), math.sin(math.radians(20)), 0)
        def miss(a):
            her_body(a); return ctx.entrance()[1].angle(nat) + math.radians(a) * 0.35
        _TUCK["spoon_arch"] = min(range(20, 75, 5), key=miss)
        print("[anim]   spoon arch", _TUCK["spoon_arch"], flush=True)
    her_body(_TUCK["spoon_arch"])
    lo = ".L" if r.head("足.L").z < r.head("足.R").z else ".R"; hi = ".R" if lo == ".L" else ".L"
    top = (0.46, 0.04, 0.24, -0.36, 0.16) if rub is not None else (0.46, 0.0, 0.17, -0.38, 0.13)   # rub: room for her hand
    side_legs(V, lo, hi, ((0.38, -0.10, 0.065, -0.36, 0.055), top), V3(0.3, 0, 0.05), settle_first=True)
    sh = r.head("腕" + lo)                              # bottom arm forward under the pillow
    hands_to(V, lo, V3(sh.x + 0.12, sh.y + 0.26, BED_TOP + 0.05), V3(0, 0, -1), nrm(V3(0.3, 1, 0)), sh + V3(0.35, 0.05, 0.12), 0.08)
    if rub is not None:
        her_rub_hand(V, hi, rub, V.lm("clit"))
    else:
        sh = r.head("腕" + hi)
        hands_to(V, hi, V3(sh.x + 0.30, sh.y - 0.02, BED_TOP + 0.045), V3(0, 0, -1), nrm(V3(0.4, 1, 0)), sh + V3(0.2, -0.1, 0.3), 0.4)
    V.look(V3(1.2, 2.2, BED_TOP + 0.25))
    E, D = ctx.entrance()
    m.reset()
    M.place(E - D * 0.25 + V3(-0.10, 0, 0), V3(1, 0, 0), V3(0, 1, 0))
    align_pelvis_to(M, D, up_hint=V3(0, 1, 0))
    def his_body(roll):                                 # scored on his finished body: chest roll, legs, bottom arm
        M.spine(forward=V3(1, 0.05, 0), up=nrm(V3(-0.1, 1, roll * 0.5)), chest_forward=V3(1, 0, 0), chest_up=nrm(V3(0, 1, roll)))
        ctx.penetrate(t)
        mlo = ".L" if m.head("足.L").z < m.head("足.R").z else ".R"; mhi = ".R" if mlo == ".L" else ".L"
        side_legs(M, mlo, mhi, ((0.30, -0.30, 0.065, -0.38, 0.06), (0.36, -0.22, 0.18, -0.40, 0.15)), V3(0.4, 0, 0.05))
        sh = m.head("腕" + mlo)                         # his bottom arm up under the pillow, above her head
        hands_to(M, mlo, V3(sh.x + 0.06, sh.y + 0.34, BED_TOP + 0.095), V3(0, 0, -1), nrm(V3(0.2, 1, 0)), sh + V3(0.3, 0.0, 0.15), 0.02)
    best_of("chest", [0.16, 0.24, 0.32, 0.40], his_body, (M,))
    mlo = ".L" if m.head("足.L").z < m.head("足.R").z else ".R"; mhi = ".R" if mlo == ".L" else ".L"
    her_hi = hi
    def his_top_hand(where):
        sh_ = m.head("腕" + mhi)
        if where == "breast":                          # cupped on her breast's real surface, from below-outside
            cup(M, mhi, surface(V), V.lm("nipple" + her_hi) + V3(0.02, -0.035, 0.02), nrm(V3(0.2, 1, 0.3)), sh_ + V3(0, -0.3, 0.3))
        else:
            T = V.lm("hip" + her_hi) + V3(0.0, 0.0, 0.04)
            cup(M, mhi, surface(V), T, nrm(V3(1, 0.3, 0)), sh_ + V3(-0.1, -0.2, 0.4))
    best_of("hand", ["breast", "hip"], his_top_hand, (M,))
    M.look(r.head("頭"))

def prone(ctx, t):
    V, M = ctx.V, ctx.M; r = V.rig
    def build(tilt):
        r.reset(); V.place(V3(0, 1.02 + 0.01 * t, BED_TOP + 0.13), nrm(yz(-90 - tilt)), nrm(yz(-tilt)))
    def cond(E, D, B):
        return max(0.0, (E.z + 0.04) - B.z) * 10 + max(0.0, B.y - (E.y - 0.06)) * 5
    solve_tilt(ctx, build, cond, range(-30, 31, 5))
    V.spine(forward=V3(0, 0.05, -1), up=V3(0, 1, 0.05), chest_forward=V3(0, 0.1, -1), chest_up=V3(0, 1, 0.1))
    SC.settle(V, sink=0.009)
    for s, sx in sides(V, "腕"):
        sh = r.head("腕" + s)
        hands_to(V, s, V3(sx * 0.16, sh.y + 0.30, BED_TOP + 0.10), V3(0, 0, -1), nrm(V3(-sx * 0.3, 1, -0.1)), sh + V3(sx * 0.4, 0, -0.2), 0.45 + 0.25 * ss(0.45, 1.0, t))
    V.look(V3(0.8, 2.4, BED_TOP + 0.1))
    E, D = ctx.entrance()
    m = M.rig; m.reset()
    M.place(E - D * 0.25 + V3(0, 0, 0.1), V3(0, 0.5, -1), V3(0, 1, 0.5))
    align_pelvis_to(M, D, up_hint=V3(0, 1, 0.45))
    def his_chest(c):
        M.spine(forward=nrm(V3(0, 0.3, -1)), up=nrm(V3(0, 1, 0.3)), chest_forward=nrm(V3(0, c, -1)), chest_up=nrm(V3(0, 1, c)))
        ctx.penetrate(t, lins=PRONE_LINS)
    best_of("chest", [0.45, 0.7, 1.0, 1.4], his_chest, (M,))
    def legs(v):
        fw, kw = v
        prone_legs(V, fw, back=0.82, knee_out=0.03 if fw < 0.2 else 0.12)
        kneel(M, kw, -1, V3(0, -1, 0.04), knee_z=0.05)
    best_of("legs", [(0.05, 0.46), (0.05, 0.52), (0.05, 0.58), (0.05, 0.64), (0.32, 0.12), (0.40, 0.15), (0.48, 0.18),
                     (0.52, 0.10), (0.56, 0.13), (0.60, 0.15)], legs, (V, M))
    for s, sx in sides(M, "腕"):
        sh = m.head("腕" + s)
        hands_to(M, s, V3(sx * 0.32, sh.y + 0.10, BED_TOP + 0.085), V3(0, 0, -1), nrm(V3(sx * 0.2, 1, 0)), sh + V3(sx * 0.5, -0.2, 0.3), 0.05,
                 plant=True)                                         # he holds himself up on his hands
    M.look(r.head("頭"))

P.CLIPS.update({
    "legsup.in":       (lambda c, t: legsup(c, t), 25, "depth"),
    "doggy.in":        (lambda c, t: doggy(c, t), 25, "depth"),
    "doggy.rub":       (lambda c, t: doggy(c, 0.5, rub=t), 25, "loop"),
    "prone.in":        (lambda c, t: prone(c, t), 25, "depth"),
    "cowgirl.in":      (lambda c, t: cowgirl(c, t), 25, "depth"),
    "cowgirl.rub":     (lambda c, t: cowgirl(c, 0.5, rub=t), 25, "loop"),
    "revcowgirl.in":   (lambda c, t: cowgirl(c, t, reverse=True), 25, "depth"),
    "revcowgirl.rub":  (lambda c, t: cowgirl(c, 0.5, reverse=True, rub=t), 25, "loop"),
    "spoon.in":        (lambda c, t: spoon(c, t), 25, "depth"),
    "spoon.rub":       (lambda c, t: spoon(c, 0.5, rub=t), 25, "loop"),
    "oral.in":         (lambda c, t: oral(c, t), 25, "depth"),
    "lick.in":         (lambda c, t: lick(c, t), 25, "depth"),
})
for k in ("doggy.rub", "cowgirl.rub", "revcowgirl.rub", "spoon.rub"):
    P.LAYER_BONES[k] = {"vivian": "arm.R"}
