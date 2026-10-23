# Remaining positions: doggy, cowgirl / reverse cowgirl, legs-up, oral, lick, spoon, prone.
# Registers into positions.CLIPS / LAYER_BONES on import. Rule everywhere: place torso -> settle on core
# anatomy -> legs/arms to world targets (so feet/hands/knees are planted after the body found the mattress).
import math
from mathutils import Vector, Matrix
import positions as P
from positions import V3, nrm, yz, LINS, TIP_OUT, supine_her, her_arms_overhead, her_rub_hand, align_pelvis_to
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

def kneel(body, knee_w, forward_of_hip, shin_dir, knee_z=0.05, foot_toes=None, foot_up=V3(0, 0, -1)):
    """knees on the mattress at +-knee_w, `forward_of_hip` (+1 knees ahead of the hips along +y, -1 behind).
    returns how far (m) the knees float above their target when the hips are out of reach"""
    r = body.rig; gap = 0.0
    for s, sx in ((".L", 1), (".R", -1)):
        H = r.head("足" + s); L = body.len["thigh" + s]
        dz = H.z - (BED_TOP + knee_z); dx = sx * knee_w - H.x
        rest = L * L - dz * dz - dx * dx
        if rest < 0: gap = max(gap, math.sqrt(dz * dz + dx * dx) - L)
        dy = math.sqrt(max(1e-4, rest))
        K = V3(sx * knee_w, H.y + forward_of_hip * dy, BED_TOP + knee_z)
        body.leg_aim(s, K, shin_dir if not callable(shin_dir) else shin_dir(sx))
        body.foot(s, foot_toes(sx) if callable(foot_toes) else (foot_toes or nrm(V3(0, -1, -0.15))), foot_up)
    return gap

def hands_to(body, s, target, palm, fingers, pole, curl=0.4):
    body.arm_ik(s, target, pole); body.hand(s, palm=palm, fingers=fingers); body.fingers(s, curl)

def supine_him(M, pelvis_y, feet_w=0.22, knees=0.12):
    r = M.rig; r.reset()
    M.place(V3(0, pelvis_y, BED_TOP + 0.10), V3(0, 0, 1), V3(0, 1, 0))
    M.spine(forward=V3(0, 0.02, 1), up=V3(0, 1, -0.02), chest_forward=V3(0, 0.15, 1), chest_up=V3(0, 1, -0.15))
    SC.settle(M, sink=0.012)
    for s, sx in ((".L", 1), (".R", -1)):
        H = r.head("足" + s)
        A = V3(sx * feet_w, H.y - 0.82 + knees, BED_TOP + 0.075)
        M.leg_ik(s, A, H + V3(sx * 0.1, -0.3, 0.5))
        M.foot(s, nrm(V3(sx * 0.3, -0.4, 1)), V3(0, -1, 0.2))

def prone_legs(body, feet_w, back=0.85, knee_out=0.1):
    r = body.rig
    for s, sx in ((".L", 1), (".R", -1)):
        H = r.head("足" + s)
        body.leg_ik(s, V3(sx * feet_w, H.y - back, BED_TOP + 0.06), H + V3(sx * knee_out, -0.4, -0.5))
        body.foot(s, nrm(V3(0, -1, -0.6)), V3(0, -0.2, -1))

# ---------------------------------------------------------------- DOGGY
def doggy(ctx, t, rub=None):
    V, M = ctx.V, ctx.M; r = V.rig; r.reset()
    V.place(V3(0, 1.02 + 0.012 * t, BED_TOP + 0.43), nrm(yz(-110)), nrm(yz(-20)))
    sh_target = V3(0, 1.02 + 0.58, BED_TOP + 0.30)
    chest_up = (sh_target - r.head("上半身2")).normalized()
    chest_fwd = chest_up.cross(V3(1, 0, 0)).normalized()
    if chest_fwd.z > 0: chest_fwd = -chest_fwd
    V.spine(forward=nrm(yz(-100)), up=nrm(yz(-10)), chest_forward=chest_fwd, chest_up=chest_up)
    kneel(V, 0.21, -1, lambda sx: nrm(V3(sx * 0.12, -1, 0.05)), knee_z=0.065, foot_toes=lambda sx: nrm(V3(sx * 0.15, -1, -0.2)))
    for s, sx in ((".L", 1), (".R", -1)):
        sh = r.head("腕" + s)
        if rub is not None and s == ".R":
            her_rub_hand(V, s, rub, V.lm("clit")); continue
        hands_to(V, s, V3(sx * 0.15, sh.y + 0.24, BED_TOP + 0.065), V3(0, 0, -1), nrm(V3(sx * 0.1, 1, 0)), sh + V3(sx * 0.2, -0.1, -0.6), 0.35)
    V.look(V3(0, 2.6, BED_TOP + 0.6))
    E, D = ctx.entrance()
    m = M.rig; m.reset()
    M.place(E - D * 0.25, V3(0, 1, -0.2), V3(0, 0.2, 1))
    align_pelvis_to(M, D, up_hint=V3(0, 0.25, 1))
    M.spine(forward=nrm(yz(-20)), up=nrm(yz(70)), chest_forward=nrm(yz(-28)), chest_up=nrm(yz(62)))
    ctx.penetrate(t)
    kneel(M, 0.10, -1, V3(0, -1, 0.04), knee_z=0.03)
    for s, sx in ((".L", 1), (".R", -1)):
        hp = V.lm("hip" + s)
        hands_to(M, s, hp + V3(sx * 0.02, -0.03, 0.07), V3(-sx, 0, -0.3), nrm(V3(-sx * 0.2, 0.5, -1)), m.head("腕" + s) + V3(sx * 0.5, -0.2, -0.1), 0.55)
    M.look(r.head("上半身2"))

# ---------------------------------------------------------------- COWGIRL / REVERSE COWGIRL
COW_STROKE = 0.075
def cowgirl(ctx, t, reverse=False, rub=None):
    V, M = ctx.V, ctx.M
    supine_him(M, pelvis_y=1.0, feet_w=0.10, knees=0.04)
    m = M.rig
    S = nrm(V3(0, -0.30 if reverse else 0.30, 0.95))
    m.aim("竿根", S)
    R0 = m.head("竿根")
    r = V.rig; r.reset()
    face = V3(0, -1, 0) if reverse else V3(0, 1, 0)
    V.place(R0 + V3(0, 0, 0.12), face, V3(0, 0, 1))
    align_to(V, "下半身", "vag_D", S, up_hint=V3(0, -0.2 if reverse else 0.2, 1))
    tip_in = t * COW_STROKE + 0.035 - TIP_OUT                         # she rides on the upper part of the shaft
    V.move(R0 + S * (ctx.L_erect - tip_in) - V.lm("vag_E"))
    lean = 18
    upv = nrm(V3(0, (-1 if reverse else 1) * math.sin(math.radians(lean)), math.cos(math.radians(lean))))
    V.spine(forward=nrm(face * 0.95 + V3(0, 0, -0.2)), up=upv, chest_forward=nrm(face + V3(0, 0, -0.35)), chest_up=nrm(upv + face * 0.25))
    kneel(V, 0.25, -1 if not reverse else 1, nrm(-face + V3(0, 0, 0.03)), knee_z=0.06,
          foot_toes=nrm(-face + V3(0, 0, -0.2)))
    for s, sx in ((".L", 1), (".R", -1)):
        if rub is not None and s == ".R":
            her_rub_hand(V, s, rub, V.lm("clit")); continue
        sh = r.head("腕" + s)
        if reverse:
            T = M.lm("thigh" + (".R" if s == ".L" else ".L")) + V3(0, 0, 0.05)
            hands_to(V, s, T, V3(0, 0, -1), nrm(V3(0, -1, -0.2)), sh + V3(sx * 0.4, 0.1, 0), 0.45)
        else:
            T = M.lm("chest" + (".R" if s == ".L" else ".L")) + V3(0, 0, 0.035)
            hands_to(V, s, T, V3(0, 0, -1), nrm(V3(-sx * 0.2, 1, -0.2)), sh + V3(sx * 0.5, -0.2, 0), 0.3)
    V.look(m.head("頭") if not reverse else V3(0, -1.5, 1.0))
    for s, sx in ((".L", 1), (".R", -1)):
        if reverse:
            hp = V.lm("hip" + s)
            hands_to(M, s, hp + V3(sx * 0.03, 0.02, 0.05), V3(-sx, 0, 0), nrm(V3(0, -0.3, 1)), m.head("腕" + s) + V3(sx * 0.3, 0.1, 0.5), 0.5)
        else:
            hp = V.lm("hip" + (".R" if s == ".L" else ".L"))
            hands_to(M, s, hp + V3(sx * 0.03, 0, 0.03), V3(-sx, 0, 0), nrm(V3(0, 0.3, 1)), m.head("腕" + s) + V3(sx * 0.4, -0.2, -0.3), 0.55)
    M.look(r.head("頭"))

# ---------------------------------------------------------------- LEGS UP (her legs raised in a V, he holds her ankles)
def legsup(ctx, t):
    V, M = ctx.V, ctx.M; r = V.rig; r.reset()
    V.place(V3(0, 0.98 + 0.012 * t, BED_TOP + 0.13), nrm(yz(90 - 22)), nrm(yz(-22)))
    V.spine(forward=V3(0, 0.15, 1), up=V3(0, 1, -0.15), chest_forward=V3(0, 0.08, 1), chest_up=V3(0, 1, -0.08))
    SC.settle(V, sink=0.012)
    her_arms_overhead(V)
    V.look(V3(0, 0.75, BED_TOP + 0.95))
    E, D = ctx.entrance()
    m = M.rig; m.reset()
    M.place(E - D * 0.25, V3(0, 1, -0.3), V3(0, 0.3, 1))
    align_pelvis_to(M, D, up_hint=V3(0, 0.3, 0.95))
    M.spine(forward=nrm(yz(-8)), up=nrm(yz(82)), chest_forward=nrm(yz(-12)), chest_up=nrm(yz(78)))
    ctx.penetrate(t)
    kneel(M, 0.16, -1, V3(0, -1, 0.04), knee_z=0.04)
    for s, sx in ((".L", 1), (".R", -1)):                           # her legs up in a V, ankles beside his head
        H = r.head("足" + s)
        A = m.head("頭") + V3(sx * 0.30, 0.02, 0.05)
        V.leg_ik(s, A, H + V3(sx * 0.4, 0.2, 0.3))
        V.foot(s, nrm(V3(sx * 0.3, 0.2, 1)), V3(0, 1, 0))
    for s, sx in ((".L", 1), (".R", -1)):                           # his hands hold her ankles
        A = r.head("足首" + s)
        hands_to(M, s, A + V3(-sx * 0.01, -0.03, -0.05), V3(-sx, 0.2, 0), nrm(V3(0, 0, 1)), m.head("腕" + s) + V3(sx * 0.5, -0.2, -0.2), 0.75)
    M.look(r.head("頭"))

# ---------------------------------------------------------------- ORAL (she kneels between his legs) / LICK
def oral(ctx, t):
    V, M = ctx.V, ctx.M
    supine_him(M, pelvis_y=1.20, feet_w=0.38, knees=0.10)
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
    kneel(V, 0.13, -1, V3(0, -1, 0.04), knee_z=0.06)
    for s, sx in ((".L", 1), (".R", -1)):
        sh = r.head("腕" + s)
        if s == ".R":
            hands_to(V, s, R0 + S * 0.035 + V3(-0.035, -0.015, 0), V3(1, 0, 0), S, sh + V3(-0.3, -0.25, -0.2), 0.75)
        else:
            hands_to(V, s, M.lm("thigh.L") + V3(0.03, 0, 0.06), V3(0, 0, -1), nrm(V3(0.3, 1, 0)), sh + V3(0.4, -0.2, -0.2), 0.35)
    for s, sx in ((".L", 1), (".R", -1)):
        sh = m.head("腕" + s)
        hands_to(M, s, V3(sx * 0.36, sh.y + 0.05, BED_TOP + 0.05), V3(0, 0, -1), nrm(V3(sx, 0.2, 0)), sh + V3(sx * 0.3, -0.3, 0.1), 0.5)
    M.look(r.head("頭"))

def lick(ctx, t):
    V, M = ctx.V, ctx.M
    supine_her(V, pelvis_y=1.05, tilt=-12, knees_w=0.80, feet_w=0.50, feet_back=0.36, head_target=V3(0, 0.7, BED_TOP + 0.4))
    her_arms_overhead(V)
    r = V.rig; m = M.rig; m.reset()
    E, D = ctx.entrance()
    M.place(V3(0, E.y - 0.80, BED_TOP + 0.12), V3(0, 0, -1), V3(0, 1, 0))
    M.spine(forward=nrm(V3(0, 0.3, -1)), up=nrm(V3(0, 1, 0.3)), chest_forward=nrm(V3(0, 0.6, -1)), chest_up=nrm(V3(0, 1, 0.6)))
    SC.settle(M, sink=0.012)
    face = nrm(D * 0.6 + V3(0, 0.4, -0.2))
    up = nrm(V3(0, 0, 1) - face * face.dot(V3(0, 0, 1)))
    m.orient("首", forward=nrm(face + V3(0, 0.2, 0)), up=up); m.orient("頭", forward=face, up=up)
    stroke = math.sin(t * math.pi) * 0.016
    delta = E - D * 0.004 + V3(0, 0.012 - stroke, 0.006) - M.lm("mouth")
    m.rotate_world("上半身", Matrix.Identity(3))
    M.move(delta)                                                   # small reach; legs re-planted below
    prone_legs(M, 0.20, back=0.88, knee_out=0.08)
    for s, sx in ((".L", 1), (".R", -1)):
        th = r.head("ひざ" + s) * 0.45 + r.head("足" + s) * 0.55
        hands_to(M, s, th + V3(sx * 0.03, 0, 0.065), V3(-sx * 0.3, 0, -1), nrm(V3(0, 0.6, 0.2)), m.head("腕" + s) + V3(sx * 0.4, -0.3, -0.25), 0.5)
    V.look(m.head("頭"))

# ---------------------------------------------------------------- SPOON / PRONE (her pelvis tilt solved numerically)
def solve_tilt(ctx, build_her, cond, tilts):
    best = None
    for tl in tilts:
        build_her(tl)
        E, D = ctx.entrance(); B = E - D * ctx.L_erect
        score = cond(E, D, B)
        if best is None or score < best[0]: best = (score, tl)
    build_her(best[1]); return best[1]

def spoon(ctx, t, rub=None):
    V, M = ctx.V, ctx.M; r = V.rig
    def build(tilt):
        r.reset()
        c, s_ = math.cos(math.radians(tilt)), math.sin(math.radians(tilt))
        f = nrm(V3(1, 0, 0) * c + V3(0, -1, 0) * s_); u = nrm(V3(0, 1, 0) * c + V3(1, 0, 0) * s_)
        V.place(V3(0.05, 1.0 + 0.01 * t, BED_TOP + 0.20), f, u)
    def cond(E, D, B):
        return max(0.0, B.x - (E.x - 0.05)) * 10 + max(0.0, B.y - (E.y - 0.05)) * 5 + abs(D.z) * 0.5
    solve_tilt(ctx, build, cond, range(-10, 75, 5))
    V.spine(forward=V3(1, 0.15, 0), up=V3(0, 1, 0), chest_forward=V3(1, 0.05, 0), chest_up=V3(0, 1, 0))
    SC.settle(V, sink=0.015)
    for s, knee_fwd in ((".L", 0.35), (".R", 0.55)):
        H = r.head("足" + s)
        K = H + V3(knee_fwd, -0.12, (0.0 if s == ".L" else 0.14))
        V.leg_ik(s, K + V3(-0.05, -0.38, 0.0 if s == ".L" else -0.1), K + V3(0.3, 0, 0))
        V.foot(s, nrm(V3(0.3, -1, 0)), V3(1, 0, 0.2))
    for s in (".L", ".R"):
        sh = r.head("腕" + s)
        if rub is not None and s == ".R":
            her_rub_hand(V, s, rub, V.lm("clit")); continue
        hands_to(V, s, sh + V3(0.30, 0.18, 0.0 if s == ".R" else -0.08), V3(0, 0, -1), nrm(V3(1, 0.4, 0)), sh + V3(-0.1, -0.2, -0.3), 0.4)
    V.look(V3(1.2, 2.0, BED_TOP + 0.3))
    E, D = ctx.entrance()
    m = M.rig; m.reset()
    M.place(E - D * 0.25 + V3(-0.12, 0, 0), V3(1, 0.1, 0), V3(0, 1, 0))
    align_pelvis_to(M, D, up_hint=V3(0, 1, 0))
    M.spine(forward=V3(1, -0.1, 0), up=V3(0, 1, 0.05), chest_forward=V3(1, -0.15, 0.05), chest_up=V3(0, 1, 0.1))
    ctx.penetrate(t)
    for s in (".L", ".R"):
        H = m.head("足" + s)
        K = H + V3(0.28, -0.32, (0.0 if s == ".L" else 0.1))
        M.leg_ik(s, K + V3(-0.08, -0.38, 0), K + V3(0.4, 0, 0))
        M.foot(s, nrm(V3(0.2, -1, 0)), V3(1, 0, 0.2))
    for s in (".L", ".R"):
        sh = m.head("腕" + s)
        if s == ".R":
            hands_to(M, s, V.lm("nipple.R") + V3(0.03, -0.05, 0.03), V3(-1, 0, 0), nrm(V3(0.2, 1, 0)), sh + V3(0, -0.3, 0.3), 0.45)
        else:
            hands_to(M, s, sh + V3(0.25, 0.25, -0.05), V3(0, 0, -1), nrm(V3(0.5, 1, 0)), sh + V3(-0.2, -0.2, -0.3), 0.3)
    M.look(r.head("頭"))

def prone(ctx, t):
    V, M = ctx.V, ctx.M; r = V.rig
    def build(tilt):
        r.reset(); V.place(V3(0, 1.02 + 0.01 * t, BED_TOP + 0.13), nrm(yz(-90 - tilt)), nrm(yz(-tilt)))
    def cond(E, D, B):
        return max(0.0, (E.z + 0.04) - B.z) * 10 + max(0.0, B.y - (E.y - 0.06)) * 5
    solve_tilt(ctx, build, cond, range(-30, 31, 5))
    V.spine(forward=V3(0, 0.05, -1), up=V3(0, 1, 0.05), chest_forward=V3(0, 0.1, -1), chest_up=V3(0, 1, 0.1))
    SC.settle(V, sink=0.015)
    prone_legs(V, 0.12, back=0.82)
    for s, sx in ((".L", 1), (".R", -1)):
        sh = r.head("腕" + s)
        hands_to(V, s, V3(sx * 0.16, sh.y + 0.30, BED_TOP + 0.10), V3(0, 0, -1), nrm(V3(-sx * 0.3, 1, 0)), sh + V3(sx * 0.4, 0, -0.2), 0.45)
    V.look(V3(0.8, 2.4, BED_TOP + 0.1))
    E, D = ctx.entrance()
    m = M.rig; m.reset()
    M.place(E - D * 0.25 + V3(0, 0, 0.1), V3(0, 0.5, -1), V3(0, 1, 0.5))
    align_pelvis_to(M, D, up_hint=V3(0, 1, 0.45))
    M.spine(forward=nrm(V3(0, 0.3, -1)), up=nrm(V3(0, 1, 0.3)), chest_forward=nrm(V3(0, 0.45, -1)), chest_up=nrm(V3(0, 1, 0.45)))
    ctx.penetrate(t)
    prone_legs(M, 0.32, back=0.85, knee_out=0.15)
    for s, sx in ((".L", 1), (".R", -1)):
        sh = m.head("腕" + s)
        hands_to(M, s, V3(sx * 0.32, sh.y + 0.10, BED_TOP + 0.04), V3(0, 0, -1), nrm(V3(sx * 0.2, 1, 0)), sh + V3(sx * 0.5, -0.2, 0.3), 0.2)
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
