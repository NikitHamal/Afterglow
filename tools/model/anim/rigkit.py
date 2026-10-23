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
from mathutils import Matrix, Vector, Quaternion

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

    def ik2(self, upper, lower, end, target, pole, reach=0.999):
        """analytic two-bone IK. upper/lower: bone names; end: the bone whose head is the end effector
        (wrist/ankle). The hinge (both bones' X axes) is kept perpendicular to the bend plane on the
        side nearest the current hinge, so knees/elbows never twist. Returns the residual distance."""
        S = self.head(upper); E0 = self.head(lower); W0 = self.head(end)
        L1 = (E0 - S).length; L2 = (W0 - E0).length
        T = Vector(target); d = T - S; dist = d.length
        dist_c = max(abs(L1 - L2) + 1e-4, min(dist, (L1 + L2) * reach))
        dn = d.normalized()
        pv = Vector(pole) - S; pv = pv - dn * pv.dot(dn)
        if pv.length < 1e-6: pv = (E0 - S) - dn * (E0 - S).dot(dn)
        pv.normalize()
        a = (L1 * L1 + dist_c * dist_c - L2 * L2) / (2 * L1 * dist_c); a = max(-1.0, min(1.0, a))
        ang = math.acos(a)
        E = S + (dn * math.cos(ang) + pv * math.sin(ang)) * L1
        W = S + dn * dist_c
        hinge = (E - S).cross(W - E)
        if hinge.length < 1e-8: hinge = dn.cross(pv)
        hinge.normalize()
        for b, seg in ((upper, E - S), (lower, None)):
            if seg is None: seg = W - self.head(lower)
            hl = self.hinge.get(b, Vector((1, 0, 0)))
            # keep the anatomical sense of the hinge continuous with the bone's current frame
            R_cur = self.P[b].to_3x3().normalized()
            R_min = self.axes(b)[1].rotation_difference(seg.normalized()).to_matrix() @ R_cur
            h = hinge if hinge.dot(R_min @ hl) >= 0 else -hinge
            self.aim2(b, seg, h, hl)
        return (self.head(end) - T).length

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
        pbs = self.arm.pose.bones
        for n in (bones or [b for b in self.order if not b.startswith(self.SKIP_KEY)]):
            pb = pbs[n]; pb.rotation_mode = "QUATERNION"
            pb.matrix_basis = self.basis[n]
            pb.keyframe_insert("location", frame=frame, group=n)
            pb.keyframe_insert("rotation_quaternion", frame=frame, group=n)

    def snapshot(self):
        return {n: m.copy() for n, m in self.basis.items()}

    def restore(self, snap):
        for n, m in snap.items(): self.basis[n] = m.copy()
        self.fk()
