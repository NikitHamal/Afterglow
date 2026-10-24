# rigkit — a small deterministic pose solver for authoring baked clips on the MMD rigs (Blender).
#
# Works entirely in armature space with our own forward kinematics, so posing never needs depsgraph
# updates: every bone keeps a local `basis` (the pose-bone matrix_basis) and P[name] = armature-space
# pose matrix. Posing helpers speak anatomy, not bone rolls:
#   place(pelvis_loc, forward, up)       whole body, rigidly, by its pelvis
#   orient(bone, forward=, up=)          rotate a bone so its rest "front"/"up" map to the given world dirs
#   aim(bone, ydir, xhint)               bone axis to a direction (twist from xhint or minimal rotation)
#   ik2(upper, lower, target, pole)      analytic two-bone IK, hinge axis kept perpendicular to the bend plane
#   hand(side, palm=, fingers=)          hand orientation from palm normal + finger direction
#   curl(side, amount, spread)           finger curl toward the palm (curl axes come from rig_export)
#   key(frame)                           write matrix_basis of every bone as keyframes on the active action
# Armature objects are expected at identity transform (rig_export leaves them there).
import bpy, math
from mathutils import Quaternion, Matrix, Vector, Quaternion

WORLD_FWD, WORLD_UP, WORLD_LEFT = Vector((0, -1, 0)), Vector((0, 0, 1)), Vector((1, 0, 0))  # character rest frame (faces -Y)

def _norm(v):
    v = Vector(v); n = v.length
    return v / n if n > 1e-9 else v

def frame_from(y, xhint):
    """rotation matrix with Y=y and X as close as possible to xhint"""
    y = _norm(y); x = Vector(xhint) - y * y.dot(xhint)
    if x.length < 1e-6:
        x = y.orthogonal()
    x.normalize(); z = x.cross(y)
    return Matrix((x, y, z)).transposed()

class Rig:
    def __init__(self, arm):
        assert arm.type == "ARMATURE"
        self.arm = arm
        self.bones = {b.name: b for b in arm.data.bones}
        self.parent = {n: (b.parent.name if b.parent else None) for n, b in self.bones.items()}
        self.children = {n: [] for n in self.bones}
        for n, p in self.parent.items():
            if p: self.children[p].append(n)
        self.order = []
        def walk(n):
            self.order.append(n)
            for c in self.children[n]: walk(c)
        for n, p in self.parent.items():
            if p is None: walk(n)
        self.rest = {n: b.matrix_local.copy() for n, b in self.bones.items()}
        self.rest_rel = {n: (self.rest[p].inverted() @ self.rest[n]) if p else self.rest[n].copy() for n, p in self.parent.items()}
        self.length = {n: b.length for n, b in self.bones.items()}
        self.basis = {n: Matrix.Identity(4) for n in self.bones}
        self.P = {}
        self.fk()
        # anatomical reference vectors in each bone's local frame (from the rest pose: body faces -Y, up +Z, left +X)
        self.loc_fwd, self.loc_up, self.loc_left = {}, {}, {}
        for n in self.bones:
            R = self.rest[n].to_3x3().inverted()
            self.loc_fwd[n] = (R @ WORLD_FWD).normalized()
            self.loc_up[n] = (R @ WORLD_UP).normalized()
            self.loc_left[n] = (R @ WORLD_LEFT).normalized()
        self.palm = {}      # local palm normal per hand bone (palms_from_fingers / set_palms)
        self.hinge = {}     # local hinge axis per limb bone (set_hinges)

    # ---------------------------------------------------------------- FK core
    def fk(self, start=None):
        names = self.order if start is None else self.subtree(start)
        for n in names:
            p = self.parent[n]
            self.P[n] = ((self.P[p] @ self.rest_rel[n]) if p else self.rest_rel[n]) @ self.basis[n]

    def subtree(self, n):
        out = []
        def walk(m):
            out.append(m)
            for c in self.children[m]: walk(c)
        walk(n); return out

    def reset(self):
        for n in self.basis: self.basis[n] = Matrix.Identity(4)
        self.fk()

    def set_pose(self, n, M):
        """set bone n's armature-space pose matrix; descendants follow (keep their local basis)"""
        p = self.parent[n]
        parentP = self.P[p] if p else Matrix.Identity(4)
        self.basis[n] = (parentP @ self.rest_rel[n]).inverted() @ M
        self.fk(n)

    # ---------------------------------------------------------------- queries (armature space == world)
    def head(self, n): return self.P[n].translation.copy()
    def tail(self, n): return self.P[n] @ Vector((0, self.length[n], 0))
    def axes(self, n):
        R = self.P[n].to_3x3().normalized(); return R.col[0].copy(), R.col[1].copy(), R.col[2].copy()
    def world_dir(self, n, local):
        return (self.P[n].to_3x3().normalized() @ Vector(local)).normalized()
    def point(self, n, local):
        """armature-space position of a point given in bone n's rest-local frame"""
        return self.P[n] @ Vector(local)
    def to_local(self, n, world_pt):
        """express a rest-pose world point in bone n's local frame (for rigidly attached landmarks)"""
        return self.rest[n].inverted() @ Vector(world_pt)

    # ---------------------------------------------------------------- posing
    def rotate_world(self, n, R3, pivot=None):
        """apply a world-space rotation (3x3) to bone n around its head (or a pivot)"""
        M = self.P[n]; piv = self.head(n) if pivot is None else Vector(pivot)
        T = Matrix.Translation(piv) @ R3.to_4x4() @ Matrix.Translation(-piv)
        self.set_pose(n, T @ M)

    def place(self, root, pelvis, loc, forward, up):
        """move the whole body rigidly so `pelvis` bone's head sits at loc with the body facing `forward`, `up` up"""
        f = _norm(forward); u = _norm(up - f * f.dot(up)); l = u.cross(f)          # body left = up x forward... (rest: (0,0,1)x(0,-1,0)=(1,0,0))
        Rw = Matrix((l, -f, u)).transposed()                                       # maps rest axes (X=left, Y=-fwd, Z=up) to world
        Pp = self.P[pelvis]
        cur_l, cur_f, cur_u = self.world_dir(pelvis, self.loc_left[pelvis]), self.world_dir(pelvis, self.loc_fwd[pelvis]), self.world_dir(pelvis, self.loc_up[pelvis])
        Rc = Matrix((cur_l, -cur_f, cur_u)).transposed()
        G3 = Rw @ Rc.transposed()
        G = Matrix.Translation(Vector(loc)) @ G3.to_4x4() @ Matrix.Translation(-Pp.translation)
        self.set_pose(root, G @ self.P[root])

    def translate(self, root, d):
        M = self.P[root].copy(); M.translation += Vector(d); self.set_pose(root, M)

    def orient(self, n, forward=None, up=None, keep_y=False):
        """rotate bone n about its head so its anatomical front -> forward and up -> up (both world dirs)"""
        if forward is None and up is None: return
        R_cur = self.P[n].to_3x3().normalized()
        if forward is not None and up is not None:
            f = _norm(forward); u = _norm(Vector(up) - f * f.dot(up)); l = u.cross(f)
            A = Matrix((l, f, u)).transposed()                                       # desired world of (left, fwd, up)
            lf, ff, uf = self.loc_left[n], self.loc_fwd[n], self.loc_up[n]
            B = Matrix((lf, ff, uf)).transposed()                                    # local of (left, fwd, up)
            R_new = A @ B.inverted()
        else:
            ref_local = self.loc_fwd[n] if forward is not None else self.loc_up[n]
            want = _norm(forward if forward is not None else up)
            cur = (R_cur @ ref_local).normalized()
            R_new = cur.rotation_difference(want).to_matrix() @ R_cur
        M = Matrix.Translation(self.head(n)) @ R_new.to_4x4()
        self.set_pose(n, M)

    def aim(self, n, ydir, xhint=None):
        """bone axis -> ydir; twist from xhint (projected) or the minimal rotation from the current frame"""
        x, y, z = self.axes(n)
        if xhint is None:
            R_new = y.rotation_difference(_norm(ydir)).to_matrix() @ self.P[n].to_3x3().normalized()
        else:
            R_new = frame_from(ydir, xhint)
        self.set_pose(n, Matrix.Translation(self.head(n)) @ R_new.to_4x4())

    def aim2(self, n, ydir, hint_world, hint_local):
        """bone axis -> ydir and the bone-local vector hint_local -> hint_world (both projected off the axis)"""
        y = _norm(ydir); ly = Vector((0, 1, 0))
        lh = _norm(Vector(hint_local) - ly * ly.dot(hint_local)); lz = lh.cross(ly)
        wh = _norm(Vector(hint_world) - y * y.dot(hint_world)); wz = wh.cross(y)
        R_new = Matrix((wh, y, wz)).transposed() @ Matrix((lh, ly, lz)).transposed().inverted()
        self.set_pose(n, Matrix.Translation(self.head(n)) @ R_new.to_4x4())

    # anatomical hinge sense: an elbow flexes one way (+ about the rest hinge), a knee the other (-); the old
    # continuity rule could pick the mirror solution = the same world bend with the humerus wrung ~180 deg
    FLEX_SIGN = {"ひじ": 1.0, "ひざ": -1.0}
    MAX_FLEX = {"ひじ": 150.0, "ひざ": 160.0}            # deg; past this the forearm/shin would pass through the upper limb
    ARM_TWIST_MAX = 80.0                                # humerus rotation vs rest (internal/external) a shoulder really has (deg)
    floor_z = None                                      # authoring: elbows stay above this (the mattress top + pad)
    obstacles = ()                                      # authoring: [(rig, bone_a, bone_b, radius, margin)] capsules the arm keeps out of

    def _capsule_pen(self, P):
        """deepest penetration of point P into the obstacle capsules (m)"""
        worst = 0.0
        for rig_, a_, b_, rad, mg in self.obstacles:
            A_ = rig_.head(a_) if a_ in rig_.bones else None
            if A_ is None: continue
            B_ = rig_.tail(a_) if b_ is None else rig_.head(b_)
            ab = B_ - A_; L2 = ab.dot(ab)
            u_ = 0.0 if L2 < 1e-12 else max(0.0, min(1.0, (P - A_).dot(ab) / L2))
            worst = max(worst, rad + mg - (P - (A_ + ab * u_)).length)
        return worst

    def _aim2_R(self, ydir, hint_world, hint_local):
        y = _norm(ydir); ly = Vector((0, 1, 0))
        lh = _norm(Vector(hint_local) - ly * ly.dot(hint_local)); lz = lh.cross(ly)
        wh = _norm(Vector(hint_world) - y * y.dot(hint_world)); wz = wh.cross(y)
        return Matrix((wh, y, wz)).transposed() @ Matrix((lh, ly, lz)).transposed().inverted()

    def _twist_of(self, n, R_world):
        """twist (deg) about bone n's own axis, vs rest, if bone n had world rotation R_world"""
        p = self.parent[n]
        parentR = (self.P[p] @ self.rest_rel[n]).to_3x3().normalized() if p else self.rest_rel[n].to_3x3().normalized()
        q = (parentR.inverted() @ R_world).to_quaternion(); q.normalize()
        if q.w < 0: q = -q
        return math.degrees(2.0 * math.atan2(q.y, q.w)) if abs(q.w) + abs(q.y) > 1e-9 else 0.0

    def _bend(self, S, L1, L2, T, pv, flex_max):
        """elbow/knee point for a bend toward pv (unit, perpendicular to S->T), reach clamped to the anatomical range"""
        d = T - S; dist = d.length; dn = d.normalized()
        dmin = math.sqrt(max(1e-8, L1 * L1 + L2 * L2 - 2 * L1 * L2 * math.cos(math.radians(180.0 - flex_max))))
        dist_c = max(dmin, abs(L1 - L2) + 1e-4, min(dist, (L1 + L2) * 0.999))
        a = (L1 * L1 + dist_c * dist_c - L2 * L2) / (2 * L1 * dist_c); a = max(-1.0, min(1.0, a))
        ang = math.acos(a)
        return S + (dn * math.cos(ang) + pv * math.sin(ang)) * L1, S + dn * dist_c

    def ik2(self, upper, lower, end, target, pole, reach=0.999, natural=None):
        """analytic two-bone IK with an anatomical hinge: both bones' hinge axes lie on the bend-plane normal with the
        limb's own flexion sense (FLEX_SIGN), so an elbow/knee never bends backwards and the upper bone is never
        wrung round to fake it. Arms (natural): the elbow is placed on its circle near `pole` where the humerus
        twist stays relaxed (pole intent kept when it is already natural). Returns the residual distance."""
        S = self.head(upper); E0 = self.head(lower); W0 = self.head(end)
        L1 = (E0 - S).length; L2 = (W0 - E0).length
        T = Vector(target); d = T - S
        dn = d.normalized() if d.length > 1e-9 else Vector((0, 0, -1))
        key = lower[:2]
        sign = self.FLEX_SIGN.get(key, 1.0); fmax = self.MAX_FLEX.get(key, 160.0)
        if natural is None: natural = key == "ひじ"
        hl_up = self.hinge.get(upper, Vector((1, 0, 0))); hl_lo = self.hinge.get(lower, Vector((1, 0, 0)))

        def hinge_for(E, W, pv):
            g = (E - S).cross(W - E)
            if g.length < 1e-7: g = pv.cross(dn)          # straight limb: bend plane from the pole
            if g.length < 1e-9: g = dn.orthogonal()
            return g.normalized() * sign

        pv0 = Vector(pole) - S; pv0 = pv0 - dn * pv0.dot(dn)
        if pv0.length < 1e-6: pv0 = (E0 - S) - dn * (E0 - S).dot(dn)
        if pv0.length < 1e-6: pv0 = dn.orthogonal()
        pv0.normalize()
        pv = pv0
        if natural:
            # keep the authored elbow when it is anatomical; otherwise the nearest elbow on the circle that is: humerus
            # twist within ARM_TWIST_MAX, no deeper in the mattress / either body than the authored one, never a jump
            # from the last keyed frame (continuity window)
            e1 = pv0; e2 = dn.cross(e1).normalized()
            last = getattr(self, "_frame_bend", {}).get(upper)          # last KEYED frame's elbow side (not the last call)
            lp = None
            if last is not None:
                lp = last - dn * last.dot(dn)
                lp = lp.normalized() if lp.length > 1e-6 else None
            def probe(c):
                E, W = self._bend(S, L1, L2, T, c, fmax)
                tw = self._twist_of(upper, self._aim2_R(E - S, hinge_for(E, W, c), hl_up))
                fl = max(0.0, self.floor_z + 0.035 - E.z) if self.floor_z is not None else 0.0
                ob = 0.0
                if self.obstacles:
                    for P_ in (S + (E - S) * 0.55, E, E + (W - E) * 0.2, E + (W - E) * 0.4, E + (W - E) * 0.6, E + (W - E) * 0.8):
                        ob = max(ob, self._capsule_pen(P_))
                return tw, fl, ob
            cands = []
            for k in range(-18, 18):                        # 10 deg steps round the elbow circle
                a = math.radians(10.0 * k)
                c = (e1 * math.cos(a) + e2 * math.sin(a)).normalized()
                if lp is not None and c.dot(lp) < math.cos(math.radians(45.0)): continue
                cands.append((abs(a), c) + probe(c))
            base = probe(pv0)
            ok = [x for x in cands if abs(x[2]) <= self.ARM_TWIST_MAX and x[3] <= base[1] + 0.002 and x[4] <= base[2] + 0.003]
            if lp is None and abs(base[0]) <= self.ARM_TWIST_MAX: pv = pv0
            elif ok:
                ref = pv0 if lp is None else (pv0 * 0.5 + lp * 0.5).normalized()
                pv = max(ok, key=lambda x: x[1].dot(ref))[1]   # nearest to the authored pole (and last frame)
            elif cands:
                def cost(x):
                    return (max(0.0, abs(x[2]) - self.ARM_TWIST_MAX) / 15.0) ** 2 * 5.0 + x[0] * 0.5 + 4000.0 * x[3] + 2500.0 * x[4]
                pv = min(cands, key=cost)[1]
            elif lp is not None: pv = lp
            self.__dict__.setdefault("_last_bend", {})[upper] = pv.copy()
        E, W = self._bend(S, L1, L2, T, pv, fmax)
        h = hinge_for(E, W, pv)
        self.aim2(upper, E - S, h, hl_up)
        self.aim2(lower, W - self.head(lower), h, hl_lo)
        return (self.head(end) - T).length

    def end_frame(self):
        """a frame was keyed: its elbow sides become the continuity reference of the next frame"""
        self._frame_bend = {k: v.copy() for k, v in getattr(self, "_last_bend", {}).items()}

    def share_twist(self, forearm, wrist, k=0.5):
        """pronation/supination lives along the forearm: hand k of the wrist's twist (about its own axis) to the
        forearm bone; the hand keeps its world orientation (the wrist sits on the forearm's axis)"""
        if forearm not in self.basis or wrist not in self.basis: return
        q = self.basis[wrist].to_quaternion(); q.normalize()
        if q.w < 0: q = -q
        tw = math.degrees(2.0 * math.atan2(q.y, q.w))
        mem = self.__dict__.setdefault("_last_share", {})
        if wrist in mem and abs(tw - mem[wrist]) > 180.0: tw += math.copysign(360.0, mem[wrist] - tw)   # no wrap: no flip
        mem[wrist] = tw
        if abs(tw) < 1.0: return
        Pw = self.P[wrist].copy()
        self.basis[forearm] = self.basis[forearm] @ Matrix.Rotation(math.radians(k * tw), 4, "Y")
        self.fk(forearm)
        self.set_pose(wrist, Pw)

    def set_hinges(self, limbs):
        """limbs: [(upper, lower), ...]; the rest hinge (perpendicular to the limb and the body's forward
        axis) is stored in each bone's local frame so IK bends keep the rest twist"""
        self.hinge = getattr(self, "hinge", {})
        for up, lo in limbs:
            a = (self.rest[lo].translation - self.rest[up].translation).normalized()
            hw = a.cross(WORLD_FWD)
            if hw.length < 1e-6: hw = WORLD_LEFT.copy()
            hw.normalize()
            for b in (up, lo):
                self.hinge[b] = (self.rest[b].to_3x3().inverted() @ hw).normalized()

    def palms_from_fingers(self, hand_bones):
        """palm normal of each hand bone = mean Z axis of its finger base bones in rest (rig_export rolls finger Z to the palm)"""
        for hand, fingers in hand_bones.items():
            v = Vector()
            for f in fingers:
                if f in self.bones: v += self.rest[f].to_3x3().col[2]
            if v.length > 0: self.palm[hand] = (self.rest[hand].to_3x3().inverted() @ v.normalized()).normalized()

    # ---------------------------------------------------------------- hands, feet, fingers
    def set_palms(self, palms):
        """palms: {bone_name: world palm normal in REST pose} -> stored in bone-local frames"""
        for n, v in palms.items():
            if n in self.bones: self.palm[n] = (self.rest[n].to_3x3().inverted() @ Vector(v)).normalized()

    def hand(self, n, palm, fingers):
        """orient hand bone n: its axis toward `fingers` (world dir), palm normal toward `palm`"""
        y = _norm(fingers); pl = self.palm.get(n)
        if pl is None:
            self.aim(n, y); return
        # build rotation mapping local Y -> y and local palm -> palm (orthonormalised)
        p_w = _norm(Vector(palm) - y * y.dot(palm))
        ly = Vector((0, 1, 0)); lp = (pl - ly * ly.dot(pl)).normalized(); lx = ly.cross(lp)
        A = Matrix((y.cross(p_w), y, p_w)).transposed(); B = Matrix((lx, ly, lp)).transposed()
        R_new = A @ B.inverted()
        self.set_pose(n, Matrix.Translation(self.head(n)) @ R_new.to_4x4())

    def curl(self, fingers, amount, base_extra=0.0, decay=(1.0, 0.85, 0.65)):
        """curl finger chains toward the palm: fingers = list of chains [b0, b1, b2(, b3)]; amount in radians"""
        for chain in fingers:
            for k, b in enumerate(chain):
                if b not in self.bones: continue
                a = amount * decay[min(k, len(decay) - 1)] + (base_extra if k == 0 else 0.0)
                self.basis[b] = Matrix.Rotation(a, 4, "X")
            if chain and chain[0] in self.bones: self.fk(chain[0])

    # ---------------------------------------------------------------- output
    def apply(self):
        pbs = self.arm.pose.bones
        for n in self.order:
            pb = pbs[n]; pb.rotation_mode = "QUATERNION"
            pb.matrix_basis = self.basis[n]

    SKIP_KEY = ("髪", "目", "竿端")       # hair chains + eyes are driven live by the engine (sim/gaze); 竿端 is a marker

    def key(self, frame, bones=None):
        """key every authored bone; quaternions are kept sign-continuous with the previous key so interpolation
        never takes the long way round (q and -q are the same rotation but blend 360 degrees apart)"""
        pbs = self.arm.pose.bones
        last = getattr(self, "_lastq", None)
        if last is None or frame <= 1: last = self._lastq = {}
        for n in (bones or [b for b in self.order if not b.startswith(self.SKIP_KEY)]):
            pb = pbs[n]; pb.rotation_mode = "QUATERNION"
            pb.matrix_basis = self.basis[n]
            q = pb.rotation_quaternion.copy()
            if n in last and last[n].dot(q) < 0: q.negate(); pb.rotation_quaternion = q
            last[n] = q
            pb.keyframe_insert("location", frame=frame, group=n)
            pb.keyframe_insert("rotation_quaternion", frame=frame, group=n)

    # ---------------------------------------------------------------- anatomical range of motion
    # swing = rotation of the bone axis away from its rest direction, twist = rotation about the bone axis;
    # both relative to the parent (the rest pose is the anatomical neutral: standing, arms relaxed down)
    # wrist twist = forearm pronation/supination (the IK locks the forearm roll to the elbow hinge)
    ROM = {"首": (40.0, 45.0), "頭": (32.0, 38.0), "手首.L": (65.0, 85.0), "手首.R": (65.0, 85.0),
           "足首.L": (82.0, 35.0), "足首.R": (82.0, 35.0)}             # ankle incl. midfoot + foot roll

    def swing_twist(self, n):
        q = self.basis[n].to_quaternion(); q.normalize()
        if q.w < 0: q = -q                                        # shortest-arc hemisphere
        tw = Quaternion((q.w, 0.0, q.y, 0.0))
        if tw.magnitude < 1e-8: tw = Quaternion()
        tw.normalize()
        if tw.w < 0: tw = -tw
        sw = q @ tw.inverted()
        if sw.w < 0: sw = -sw
        def ang(x): return math.degrees(2 * math.acos(max(-1.0, min(1.0, x.w))))
        ta = ang(tw) * (1.0 if tw.y >= 0 else -1.0)
        return ang(sw), ta, sw, tw

    def rom_excess(self, n):
        if n not in self.ROM or n not in self.basis: return 0.0
        smax, tmax = self.ROM[n]; sa, ta, _, _ = self.swing_twist(n)
        return max(0.0, sa - smax) + max(0.0, abs(ta) - tmax)

    def limit(self, n, carry_twist_to=None):
        """clamp bone n to its ROM; excess twist of a wrist is handed to the forearm (pronation/supination)"""
        if n not in self.ROM: return
        smax, tmax = self.ROM[n]; sa, ta, sw, tw = self.swing_twist(n)
        if not hasattr(self, "_last_twist"): self._last_twist = {}
        last = self._last_twist.get(n)
        if last is not None and abs(ta - last) > 180.0: ta += math.copysign(360.0, last - ta)   # clamp toward last frame's side
        self._last_twist[n] = max(-tmax, min(tmax, ta))
        changed = False
        if sa > smax:
            ax, an = sw.to_axis_angle()
            if an > math.pi: an -= 2 * math.pi
            sw = Quaternion(ax, math.radians(smax) * (1 if an >= 0 else -1)); changed = True
        if abs(ta) > tmax:
            excess = math.radians(ta - math.copysign(tmax, ta))
            tw = Quaternion((0.0, 1.0, 0.0), math.radians(math.copysign(tmax, ta))); changed = True
            if carry_twist_to is not None and carry_twist_to in self.basis:
                self.basis[carry_twist_to] = self.basis[carry_twist_to] @ Quaternion((0.0, 1.0, 0.0), excess).to_matrix().to_4x4()
        if changed:
            self.basis[n] = (sw @ tw).to_matrix().to_4x4()
            self.fk(carry_twist_to if carry_twist_to in self.basis else n)

    def limit_all(self):
        for n in ("首", "頭"): self.limit(n)
        for s in (".L", ".R"):
            self.share_twist("ひじ" + s, "手首" + s, 0.5)
            self.limit("手首" + s)
            self.limit("足首" + s)

    def snapshot(self):
        return {n: m.copy() for n, m in self.basis.items()}

    def restore(self, snap):
        for n, m in snap.items(): self.basis[n] = m.copy()
        self.fk()
