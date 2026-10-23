# Body: anatomy-aware wrapper around rigkit.Rig for Vivian / Marcus (MMD bone names, real scale, Blender Z-up).
import bpy, math, json
import numpy as np
from mathutils import Vector, Matrix, Quaternion
from rigkit import Rig, _norm, WORLD_FWD, WORLD_UP, WORLD_LEFT

SIDES = {"L": ".L", "R": ".R"}
FINGERS = ("親指", "人指", "中指", "薬指", "小指")

class Body:
    def __init__(self, arm, body_obj, who):
        self.who = who; self.arm = arm; self.obj = body_obj
        self.rig = Rig(arm)
        r = self.rig
        r.set_hinges([("腕.L", "ひじ.L"), ("腕.R", "ひじ.R"), ("足.L", "ひざ.L"), ("足.R", "ひざ.R")])
        hands = {}
        for s in (".L", ".R"):
            hands["手首" + s] = [f"{f}０{s}" for f in FINGERS[1:]]
        r.palms_from_fingers(hands)
        self.len = {}
        for s in (".L", ".R"):
            self.len["thigh" + s] = (r.rest["ひざ" + s].translation - r.rest["足" + s].translation).length
            self.len["shin" + s] = (r.rest["足首" + s].translation - r.rest["ひざ" + s].translation).length
            self.len["upper" + s] = (r.rest["ひじ" + s].translation - r.rest["腕" + s].translation).length
            self.len["fore" + s] = (r.rest["手首" + s].translation - r.rest["ひじ" + s].translation).length
        self.root = "全ての親"; self.pelvis = "下半身"
        # rest landmarks (world == armature space at rest)
        self.rest_pt = {n: r.rest[n].translation.copy() for n in r.bones}
        self.landmarks = {}            # name -> (bone, local point)

    # ---------------------------------------------------------------- landmarks (points riding a bone)
    def add_landmark(self, name, bone, world_rest_pt):
        self.landmarks[name] = (bone, self.rig.to_local(bone, world_rest_pt))
    def lm(self, name):
        b, p = self.landmarks[name]; return self.rig.point(b, p)
    def lm_dir(self, name, bone, world_rest_dir):
        """direction riding a bone: store as two landmarks"""
        p0 = self.rig.rest[bone].translation
        self.add_landmark(name + "@0", bone, p0); self.add_landmark(name + "@1", bone, p0 + Vector(world_rest_dir))
    def dir(self, name):
        return (self.lm(name + "@1") - self.lm(name + "@0")).normalized()

    # ---------------------------------------------------------------- whole-body placement
    def place(self, pelvis_loc, forward, up):
        self.rig.place(self.root, self.pelvis, pelvis_loc, forward, up)

    def move(self, d):
        self.rig.translate(self.root, d)

    # ---------------------------------------------------------------- limbs
    def leg_ik(self, s, ankle, knee_pole):
        return self.rig.ik2("足" + s, "ひざ" + s, "足首" + s, ankle, knee_pole)

    def leg_aim(self, s, knee, ankle_dir):
        """thigh straight to the knee point (scaled to the thigh length), shin along ankle_dir"""
        r = self.rig; H = r.head("足" + s)
        hint = r.world_dir("足" + s, r.hinge.get("足" + s, Vector((1, 0, 0))))
        r.aim2("足" + s, Vector(knee) - H, hint, r.hinge.get("足" + s, Vector((1, 0, 0))))
        hint = r.world_dir("ひざ" + s, r.hinge.get("ひざ" + s, Vector((1, 0, 0))))
        r.aim2("ひざ" + s, Vector(ankle_dir), hint, r.hinge.get("ひざ" + s, Vector((1, 0, 0))))

    def foot(self, s, toes, up):
        """sole frame: `toes` = heel->toe line of the sole, `up` = dorsum side (world dirs). (The ankle bone itself
        points ~28 deg below the sole line: aiming the bone tipped every foot into a ballet point.)"""
        self.rig.orient("足首" + s, forward=toes, up=up)

    def foot_relaxed(self, s, point_deg=20.0, splay=None):
        """natural foot for any leg pose: neutral = sole perpendicular to the shin, toes along the shin's front,
        then pointed by point_deg (relaxed ~15-25, lying prone ~75, kneeling on the instep ~80); `splay` (world
        dir) turns the toes slightly that way (feet fall outward when relaxed)"""
        r = self.rig
        d = (r.head("足首" + s) - r.head("ひざ" + s)).normalized()          # shin, knee -> ankle
        f0 = r.world_dir("ひざ" + s, r.loc_fwd["ひざ" + s])
        if splay is not None: f0 = f0 + Vector(splay) * 0.35
        f0 = (f0 - d * d.dot(f0)).normalized(); u0 = -d
        a = math.radians(point_deg)
        r.orient("足首" + s, forward=f0 * math.cos(a) + d * math.sin(a), up=u0 * math.cos(a) + f0 * math.sin(a))

    def arm_ik(self, s, wrist, elbow_pole):
        return self.rig.ik2("腕" + s, "ひじ" + s, "手首" + s, wrist, elbow_pole)

    def hand(self, s, palm, fingers):
        self.rig.hand("手首" + s, palm, fingers)

    def fingers(self, s, curl, thumb=None, spread=0.0, index=None):
        chains = []
        for f in FINGERS[1:]:
            chains.append([f"{f}{d}{s}" for d in "０１２３"])
        grad = {"人指": 0.80, "中指": 1.0, "薬指": 1.10, "小指": 1.22}     # a relaxed hand: index straightest, pinky most curled
        for ch in chains:
            nm = ch[0][:2]
            a = index if (index is not None and nm == "人指") else min(1.0, curl * grad.get(nm, 1.0))
            self.rig.curl([ch], a)
        th = [f"親指{d}{s}" for d in "０１２"]
        self.rig.curl([th], curl * 0.5 if thumb is None else thumb, decay=(0.6, 0.9, 0.8))

    # ---------------------------------------------------------------- spine + head
    def spine(self, forward, up, chest_forward=None, chest_up=None):
        """orient lower torso (上半身) and chest (上半身2); chest defaults to a blend toward the lower torso"""
        r = self.rig
        r.orient("上半身", forward=forward, up=up)
        r.orient("上半身2", forward=chest_forward or forward, up=chest_up or up)

    def look(self, target, up_hint=None, neck_share=0.45):
        """neck + head turn toward a world point"""
        r = self.rig
        f_head = (Vector(target) - r.head("頭")).normalized()
        up = Vector(up_hint) if up_hint is not None else r.world_dir("上半身2", r.loc_up["上半身2"])
        f_chest = r.world_dir("上半身2", r.loc_fwd["上半身2"])
        f_neck = (f_chest * (1 - neck_share) + f_head * neck_share).normalized()
        r.orient("首", forward=f_neck, up=up)
        r.orient("頭", forward=f_head, up=up)

    # ---------------------------------------------------------------- helpers
    def bone_dir(self, n, local_vec):
        return self.rig.world_dir(n, local_vec)
    def up_dir(self, n): return self.rig.world_dir(n, self.rig.loc_up[n])
    def fwd_dir(self, n): return self.rig.world_dir(n, self.rig.loc_fwd[n])
    def left_dir(self, n): return self.rig.world_dir(n, self.rig.loc_left[n])
