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
_TRY = {}            # variant under test (used when SCORE re-poses at another t)
SCORE_T = None          # set while SCORE re-poses the clip at another t: best_of must not recurse, just apply
_MEMS = ("_last_hinge", "_last_bend", "_frame_bend", "_last_share", "_last_twist", "_last_pole", "_last_curl")
def _mem(rig):
    """solver continuity memory (elbow side, hinge, twist unwrap, ...): trials must not steer each other"""
    out = {}
    for n in _MEMS:
        d = getattr(rig, n, None)
        if d is not None: out[n] = {k: (v.copy() if hasattr(v, "copy") else v) for k, v in d.items()}
    return out
def _mem_set(rig, mem):
    for n in _MEMS:
        if n in mem: setattr(rig, n, {k: (v.copy() if hasattr(v, "copy") else v) for k, v in mem[n].items()})
        elif hasattr(rig, n): setattr(rig, n, {})
def best_of(key, variants, apply, bodies):
    """pose with each variant (apply(v)), keep the one with the lowest SCORE. Decided once per clip (author.py
    runs every clip near full depth first) so the choice never changes between frames."""
    k = (CURRENT, key)
    if SCORE_T is not None:
        apply(variants[_TRY.get(k, _BEST.get(k, 0))]); return None
    if k not in _BEST:
        if SCORE is None: _BEST[k] = 0
        else:
            snaps = [b.rig.snapshot() for b in bodies]; best = None; scores = []
            mems = [_mem(b.rig) for b in bodies]
            for i, v in enumerate(variants):
                for b, sn, me in zip(bodies, snaps, mems): b.rig.restore(sn); _mem_set(b.rig, me)
                _TRY[k] = i
                apply(v); sc = SCORE(); scores.append(round(sc, 1))
                if best is None or sc < best[0]: best = (sc, i)
            for b, sn, me in zip(bodies, snaps, mems): b.rig.restore(sn); _mem_set(b.rig, me)
            _BEST[k] = best[1]; _TRY.pop(k, None)
            print("[anim]   best_of", CURRENT, key, variants[best[1]], round(best[0], 1), "| all", scores, flush=True)
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

    def penetrate(self, depth, which="vag", lins=None):
        """aim his shaft along her canal and slide him so the tip is `depth` inside (pelvis translated);
        lins: insertion at depth 1 for this position (default LINS)"""
        M = self.M; r = M.rig
        E, D = self.entrance(which)
        r.aim("竿根", D)
        tip_in = depth * (LINS if lins is None else lins) - TIP_OUT
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
def supine_her(V, pelvis_y, tilt=-15.0, knees_w=0.40, feet_w=0.40, feet_back=0.40, head_target=None, arch=0.0):
    """on her back; arch (0..1) lifts her chest off the sheet (back arching as he goes deep)"""
    r = V.rig; r.reset()
    f = nrm(yz(90 + tilt)); u = nrm(yz(tilt))                     # front up (tilted toward the feet), head toward +y
    V.place(V3(0, pelvis_y, BED_TOP + 0.10), f, u)
    V.spine(forward=nrm(V3(0, 0.05 + 0.10 * arch, 1)), up=nrm(V3(0, 1, -0.05 - 0.10 * arch)),
            chest_forward=nrm(V3(0, 0.12 + 0.22 * arch, 1)), chest_up=nrm(V3(0, 1, -0.12 - 0.22 * arch)))
    SC.settle(V, sink=0.008)                                     # back + pelvis on the sheet, before the feet are planted
    for s, sx in sides(V, "足"):
        H = r.head("足" + s)
        A = V3(sx * feet_w, pelvis_y - feet_back, BED_TOP + 0.075)
        V.leg_ik(s, A, H + V3(sx * knees_w, -0.05, 0.55))
        V.foot(s, nrm(V3(sx * 0.22, -1.0, 0.0)), V3(sx * 0.06, 0, 1))          # sole flat on the sheet
    if head_target is not None: V.look(head_target)

def her_arms_overhead(V, grip=0.0, reach=0.24, width=0.30):
    """hands up beside her head; grip > 0: palms down, fingers clutching the sheet (tightens with grip)"""
    r = V.rig
    for s, sx in sides(V, "腕"):
        sh = r.head("腕" + s)
        if grip <= 0:
            W = V3(sx * 0.27, sh.y + reach, BED_TOP + 0.10)
            V.arm_ik(s, W, sh + V3(sx * 0.45, 0.05, -0.1))
            V.hand(s, palm=V3(-sx * 0.3, -0.2, 1), fingers=nrm(V3(sx * 0.15, 1, 0.1)))
            V.fingers(s, 0.42)
        else:
            W = V3(sx * width, sh.y + reach - 0.03, BED_TOP + 0.085)
            for _ in range(3):
                V.arm_ik(s, W, sh + V3(sx * 0.45, 0.0, -0.05))
                V.hand(s, palm=V3(0, 0, -1), fingers=nrm(V3(sx * 0.2, 1, -0.15)))
                V.fingers(s, 0.42 + 0.26 * grip, thumb=0.40 + 0.25 * grip)
                low = min(min(tip(V, s, f_).z for f_ in FINGERS), V.rig.tail("親指２" + s).z) - 0.008      # pad radius
                if low >= BED_TOP - 0.003: break
                W = W + V3(0, 0, BED_TOP - 0.002 - low)

# ---------------------------------------------------------------- hands that really touch (surface-aware)
LIMB_BONES = ("腕", "ひじ", "手首", "親指", "人指", "中指", "薬指", "小指")
_KEEP = {}
def surface(body, exclude=LIMB_BONES):
    """BVH of the body's current evaluated skin (limbs in `exclude` left out, so a hand can find its own body)"""
    from mathutils.bvhtree import BVHTree
    import numpy as np
    key = (body.obj.name, exclude)
    if key not in _KEEP:
        _KEEP[key] = np.array([not b.startswith(exclude) for b in SC.dominant_bone(body.obj)])
    keep = _KEEP[key]
    SC.apply_rigs(body)
    Pv, Pf = SC.evaluated(body.obj)
    return BVHTree.FromPolygons([tuple(v) for v in Pv], [f for f in Pf if all(keep[i] for i in f)])

FINGERS = ("人指", "中指", "薬指", "小指")
def natural_pole(body, s, W, palm, fingers, hint):
    """elbow pole within +-100 deg of `hint` around the shoulder->wrist axis that leaves the wrist with the least bend
    and twist (the forearm does the pronation), elbow above the mattress, close to last frame's choice"""
    r = body.rig; S = r.head("腕" + s); ax = (Vector(W) - S).normalized()
    h = Vector(hint) - S; h = (h - ax * ax.dot(h)).normalized()
    b2 = ax.cross(h)
    last = r.__dict__.setdefault("_last_pole", {}).get(s)
    hinge_mem = {k_: v_.copy() for k_, v_ in getattr(r, "_last_hinge", {}).items()}   # trials must not steer the hinge memory
    bend_mem = {k_: v_.copy() for k_, v_ in getattr(r, "_last_bend", {}).items()}
    share_mem = dict(getattr(r, "_last_share", {}))
    def trial(a):
        p = S + (h * math.cos(a) + b2 * math.sin(a)) * 0.5
        body.arm_ik(s, W, p); body.hand(s, palm=palm, fingers=fingers)
        sw_, tw, _, _ = r.swing_twist("手首" + s)
        cost = 2.0 * max(0.0, sw_ - 35.0) + 0.6 * abs(tw) + 25.0 * (1 - math.cos(a))
        cost += 1500.0 * max(0.0, BED_TOP + 0.035 - r.head("ひじ" + s).z)        # elbow never in the mattress
        if last is not None: cost += 150.0 * (p - last).length
        return cost, p, a
    best = min((trial(math.radians(20.0 * k)) for k in range(-5, 6)), key=lambda x: x[0])
    for step in (10.0, 5.0, 2.5, 1.25):                                         # refined: the elbow glides frame to
        for a in (best[2] - math.radians(step), best[2] + math.radians(step)):   # frame instead of hopping 20 deg
            c = trial(a)
            if c[0] < best[0]: best = c
    r._last_pole[s] = best[1].copy()
    r._last_hinge = hinge_mem; r._last_bend = bend_mem; r._last_share = share_mem
    return best[1]
def tip(body, s, f): return body.rig.tail(f + "３" + s)
def palm_centre(body, s):
    r = body.rig; return (r.head("手首" + s) * 0.45 + r.head("中指１" + s) * 0.55)

_HANDV = {}
def hand_skin_gap(body, s, bvh):
    """smallest signed distance of the posed hand skin (palm + fingers) to the target surface (- = inside)"""
    import numpy as np
    key = (body.obj.name, s)
    if key not in _HANDV:
        _HANDV[key] = np.where(np.array([b.startswith(("手首", "親指", "人指", "中指", "薬指", "小指")) and b.endswith(s)
                                         for b in SC.dominant_bone(body.obj)]))[0][::3]
    SC.apply_rigs(body)
    Pv, _ = SC.evaluated(body.obj)
    g = 1.0
    for i in _HANDV[key]:
        l2, n2, _, _ = bvh.find_nearest(Vector(Pv[i]))
        if l2 is not None: g = min(g, (Vector(Pv[i]) - Vector(l2)).dot(Vector(n2)))
    return g

def lift_out(body, s, bvh, W, n, pose_hand, pole, allow=-0.003, iters=3):
    """move the wrist out along n until the skinned hand rests on (not in) the target"""
    for _ in range(iters):
        g = hand_skin_gap(body, s, bvh)
        if g >= allow: break
        W = W + n * (allow - g + 0.001)
        body.arm_ik(s, W, pole); pose_hand()
    return W

def tip_gap(bvh, q):
    l2, n2, _, _ = bvh.find_nearest(q)
    return 1.0 if l2 is None else (q - Vector(l2)).dot(Vector(n2))

_TIPFRAME = {}
def touch_tips(body, s, bvh, anchor, along, pole, curl=0.22, pad=0.007, lift=0.03, iters=5, frame_key=None, free_pole=True):
    """fingertip pads (index + middle) on the skin at `anchor`, fingers lying `along` the surface, palm lifted `lift`
    off it (never through the belly). With frame_key the hand keeps one orientation + elbow for the whole clip
    (a rubbing hand only translates), so the wrist never flips. Returns the contact point."""
    r = body.rig
    r.strict_elbow = True                                    # the forearm passes between the bodies: keep it out of them
    try: return _touch_tips(body, s, bvh, anchor, along, pole, curl, pad, lift, iters, frame_key, free_pole)
    finally: r.strict_elbow = False

def _touch_tips(body, s, bvh, anchor, along, pole, curl, pad, lift, iters, frame_key, free_pole):
    r = body.rig
    P, n = smooth_nearest(bvh, anchor, radius=0.02)
    f = Vector(along); f = (f - n * n.dot(f)).normalized()
    if frame_key is not None:
        if frame_key in _TIPFRAME: n, f, pole_c = _TIPFRAME[frame_key]
        else: pole_c = None
    goal = P + n * pad
    W = goal - f * 0.15 + n * lift
    if frame_key is not None and pole_c is not None: pole = pole_c
    elif free_pole: pole = natural_pole(body, s, W, -n, (f - n * 0.35).normalized(), pole)
    if frame_key is not None and frame_key not in _TIPFRAME: _TIPFRAME[frame_key] = (n.copy(), f.copy(), pole.copy())
    for _ in range(iters):
        body.arm_ik(s, W, pole)
        body.hand(s, palm=-n, fingers=(f - n * 0.35).normalized())
        r.limit("手首" + s)
        body.fingers(s, curl, index=curl * 0.6, thumb=0.5)
        r.curl([[f"薬指{d}{s}" for d in "０１２３"]], 1.05); r.curl([[f"小指{d}{s}" for d in "０１２３"]], 1.15)   # two fingers do the work
        t_ = (tip(body, s, "人指") + tip(body, s, "中指")) * 0.5
        W = W + (goal - t_)
    def pose_hand():
        body.hand(s, palm=-n, fingers=(f - n * 0.35).normalized()); r.limit("手首" + s)
        body.fingers(s, curl, index=curl * 0.6, thumb=0.5)
        r.curl([[f"薬指{d}{s}" for d in "０１２３"]], 1.05); r.curl([[f"小指{d}{s}" for d in "０１２３"]], 1.15)
    lift_out(body, s, bvh, W, n, pose_hand, pole, allow=-0.004)
    return P

def smooth_nearest(bvh, anchor, radius=0.03):
    """nearest surface point + a normal averaged over a disc round it: a BVH gives flat face normals that jump from
    face to face, and a hand aimed along them snaps 15-25 deg between frames"""
    loc, n, _, _ = bvh.find_nearest(Vector(anchor)); P = Vector(loc); n0 = Vector(n).normalized()
    t1 = n0.orthogonal().normalized(); t2 = n0.cross(t1)
    acc = n0 * 2.0
    for k in range(12):
        a = 2 * math.pi * k / 12; rr = radius * (1.0 if k % 2 == 0 else 0.5)
        l2, n2, _, _ = bvh.find_nearest(P + (t1 * math.cos(a) + t2 * math.sin(a)) * rr + n0 * 0.01)
        if l2 is not None and Vector(n2).dot(n0) > 0.3: acc += Vector(n2)
    return P, acc.normalized()

def cup(body, s, bvh, anchor, along, pole, pad=0.007, palm_lift=0.017, iters=5):
    """palm on the skin at `anchor` (fingers `along` the surface), then every finger curls until its pad meets the
    curve (wraps a breast, a thigh, a shaft); the thumb closes from the other side"""
    r = body.rig
    P, n = smooth_nearest(bvh, anchor)
    f = Vector(along); f = (f - n * n.dot(f)).normalized()
    goal = P + n * palm_lift
    W = goal - f * 0.05
    pole = natural_pole(body, s, W, -n, f, pole)
    for _ in range(iters):
        body.arm_ik(s, W, pole)
        body.hand(s, palm=-n, fingers=f)
        r.limit("手首" + s)
        body.fingers(s, 0.05, thumb=0.15)
        W = W + (goal - palm_centre(body, s))
    for _ in range(2):                                       # concave skin (a crease): lift the hand until it clears
        worst = min(tip_gap(bvh, q) for q in [palm_centre(body, s)] + [tip(body, s, f_) for f_ in FINGERS])
        if worst >= pad * 0.5: break
        W = W + n * (pad - worst); body.arm_ik(s, W, pole); body.hand(s, palm=-n, fingers=f)
        r.limit("手首" + s); body.fingers(s, 0.05, thumb=0.15)
    lastc = r.__dict__.setdefault("_last_curl", {})
    def settle_curl(key, c):                                 # fingers move smoothly from frame to frame
        p_ = lastc.get(key); c = c if p_ is None else p_ + max(-0.12, min(0.12, c - p_)); lastc[key] = c; return c
    for fn_ in FINGERS:                                      # wrap: smallest curl whose pad reaches the skin
        chain = [f"{fn_}{d}{s}" for d in "０１２３"]
        best = 0.05
        for c in [0.05 + 0.08 * k for k in range(18)]:
            r.curl([chain], c)
            if tip_gap(bvh, tip(body, s, fn_)) <= pad: best = c; break
            best = c
        r.curl([chain], settle_curl(chain[0], best))
    th = [f"親指{d}{s}" for d in "０１２"]
    best = 0.15
    for c in [0.15 + 0.08 * k for k in range(12)]:
        r.curl([th], c, decay=(0.6, 0.9, 0.8))
        best = c
        if tip_gap(bvh, r.tail(th[-1])) <= pad: break
    r.curl([th], settle_curl(th[0], best), decay=(0.6, 0.9, 0.8))
    snap = {k_: r.basis[k_].copy() for k_ in r.basis if k_.endswith(s) and k_.startswith(("親指", "人指", "中指", "薬指", "小指"))}
    def pose_hand():
        body.hand(s, palm=-n, fingers=f); r.limit("手首" + s)
        for k_, m_ in snap.items(): r.basis[k_] = m_.copy()
        r.fk("手首" + s)
    for _ in range(2):
        g = hand_skin_gap(body, s, bvh)
        if not (0.005 < g < 0.08): break
        W = W - n * (g - 0.002); body.arm_ik(s, W, pole); pose_hand()
    lift_out(body, s, bvh, W, n, pose_hand, pole, allow=-0.004)
    return P

def her_rub_hand(V, s, phase, target, radius=0.011, bvh=None):
    """her fingertips circle on her clit, on the skin: pads touching, palm resting above the mons (not in it)"""
    r = V.rig; sx = dict(sides(V, "腕"))[s]
    a = 2 * math.pi * phase
    fwd = V.fwd_dir("下半身"); up = V.up_dir("下半身"); left = V.left_dir("下半身")
    anchor = target + (left * math.cos(a) + up * math.sin(a)) * radius + fwd * 0.01
    bvh = bvh or surface(V)
    sh = r.head("腕" + s)
    lat = sh - r.head("上半身2"); lat = (lat - fwd * fwd.dot(lat)).normalized()   # out of her torso at that shoulder
    return touch_tips(V, s, bvh, anchor, -up, sh + lat * 0.30 - fwd * 0.05 - up * 0.30, curl=0.22, pad=0.005, lift=0.032,
                      frame_key=(CURRENT, "rub", s), free_pole=False)

def her_breast_hand(V, s, phase, nipple_lm, bvh=None):
    """her hand cups her breast from below-outside, fingers wrapping the curve, squeezing gently with the phase"""
    r = V.rig; sx = dict(sides(V, "腕"))[s]
    N = V.lm(nipple_lm); fwd = V.fwd_dir("上半身2"); up = V.up_dir("上半身2"); left = V.left_dir("上半身2")
    k = 0.5 + 0.5 * math.sin(2 * math.pi * phase)
    anchor = N - up * 0.045 + left * sx * 0.025 - fwd * 0.01        # lower-outer curve of the breast
    bvh = bvh or surface(V)
    sh = r.head("腕" + s)
    cup(V, s, bvh, anchor, nrm(up * 0.9 - left * sx * 0.35), sh + left * sx * 0.3 - fwd * 0.1 - up * 0.25, palm_lift=0.010 - 0.004 * k)

def kneel_his_legs(M, knee_w=0.17, knee_z=0.055, shin_dir=V3(0, -1, 0.04)):
    r = M.rig
    for s, sx in sides(M, "足"):
        H = r.head("足" + s); L = M.len["thigh" + s]
        dz = H.z - (BED_TOP + knee_z)
        dy = math.sqrt(max(1e-4, L * L - dz * dz - (sx * knee_w - H.x) ** 2))
        K = V3(sx * knee_w, H.y - dy, BED_TOP + knee_z)
        M.leg_aim(s, K, shin_dir)
        M.foot_relaxed(s, 78)                                   # on the instep

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
    """his weight on his hands: palms flat, pressed a few mm into the mattress"""
    from positions2 import hands_to
    r = M.rig
    for s, sx in sides(M, "腕"):
        sh = r.head("腕" + s)
        hands_to(M, s, V3(sx * w, y, BED_TOP + 0.035), V3(0, 0, -1), nrm(V3(sx * 0.25, 1, 0)), sh + V3(sx * 0.6, -0.25, 0.35), 0.15, plant=True)

# ---------------------------------------------------------------- MISSIONARY
def missionary(ctx, t, rub=None, embrace=0.0):
    V, M = ctx.V, ctx.M
    k = ss(0.45, 1.0, t)                                               # how hard he is in: her body answers
    head_pt = V3(0.55 - 0.10 * k, 1.75 + 0.20 * k, BED_TOP + 0.50 + 0.10 * k)  # face turned toward the room, chin lifts as he goes deep
    supine_her(V, pelvis_y=0.98 + 0.004 * t + 0.012 * k, tilt=-24 - 3 * k, knees_w=0.95 + 0.08 * k, feet_w=0.62, feet_back=0.30,
               head_target=head_pt, arch=0.6 * k)
    SC.settle(V, sink=0.008)
    best_of("her_arms_up", [(0.24, 0.24), (0.30, 0.24), (0.22, 0.32)], lambda v: her_arms_overhead(V, grip=0.25 + 0.75 * k, reach=v[1], width=v[0]), (V,))
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
                    cup(V, s, surface(M, exclude=("手首", "親指", "人指", "中指", "薬指", "小指")), T, nrm(V3(sx * 0.3, 0.6, 0.2)), sh + V3(sx * 0.5, -0.1, b))
                    continue
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
