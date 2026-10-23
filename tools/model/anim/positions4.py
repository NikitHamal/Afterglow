# Story-mode clips (loops, both characters, same world): standing at talking distance, sitting side by side on
# the bed edge, standing embrace. Registered into positions.CLIPS on import.
import math
from mathutils import Vector, Matrix
import positions as P
from positions import V3, nrm, sides, best_of, surface, cup
from positions2 import hands_to
from scene import BED_TOP, MAT_W
import scene as SC
import numpy as np

def stand(body, loc_xy, face, sway=0.0):
    """standing on the room floor at loc_xy facing `face`, weight on both feet, arms relaxed at the sides"""
    r = body.rig; r.reset()
    body.place(V3(loc_xy[0], loc_xy[1], 1.0), nrm(face), V3(0, 0, 1))
    f = nrm(face); left = V3(0, 0, 1).cross(f).normalized()
    body.spine(forward=nrm(f + V3(0, 0, -0.02)), up=nrm(V3(0, 0, 1) + left * sway * 0.03),
               chest_forward=f, chest_up=nrm(V3(0, 0, 1) - f * 0.03))
    SC.settle(body, target_z=0.0, sink=0.0, region=np.ones(len(body.obj.data.vertices), bool))
    for s, sx in sides(body, "足"):                                  # feet flat, hip width
        H = r.head("足" + s)
        body.leg_ik(s, V3(H.x, H.y, 0.075) + f * 0.02, H + f * 0.4)
        body.foot(s, nrm(f + left * (0.15 if (H - r.head("下半身")).dot(left) > 0 else -0.15)), V3(0, 0, 1))
    SC.settle(body, target_z=0.0, sink=0.0, region=np.ones(len(body.obj.data.vertices), bool))
    for s, sx in sides(body, "腕"):                                  # arms hang, a soft bend at the elbow
        sh = r.head("腕" + s); side = (sh - r.head("上半身2")); side = (side - V3(0, 0, side.z)).normalized()
        body.arm_ik(s, sh + side * 0.07 + f * 0.04 + V3(0, 0, -0.50), sh + side * 0.2 - f * 0.3)
        body.hand(s, palm=-side, fingers=nrm(V3(0, 0, -1) + f * 0.1)); body.fingers(s, 0.35)

def stand_idle(ctx, t):
    V, M = ctx.V, ctx.M
    sway = math.sin(2 * math.pi * t)
    stand(V, (-0.42, -0.45), V3(0.85, 0.15, 0), sway)                 # at the foot of the bed, turned toward him
    stand(M, (0.42, -0.62), V3(-0.85, 0.2, 0), -sway * 0.5)
    V.look(M.rig.head("頭") + V3(0, 0, -0.02)); M.look(V.rig.head("頭"))

def sit_edge(body, x, y, lean=8.0):
    """sitting on the bed edge (right side of the bed, facing +X), thighs on the mattress, feet on the floor"""
    r = body.rig; r.reset()
    a = math.radians(lean)
    body.place(V3(x, y, BED_TOP + 0.12), V3(1, 0, 0), V3(0, 0, 1))
    body.spine(forward=nrm(V3(math.cos(a), 0, -math.sin(a))), up=nrm(V3(math.sin(a), 0, math.cos(a))),
               chest_forward=nrm(V3(math.cos(a), 0, -math.sin(a) * 0.5)), chest_up=nrm(V3(math.sin(a) * 0.5, 0, 1)))
    SC.settle(body, target_z=BED_TOP, sink=0.012)
    for s, sx in sides(body, "足"):
        H = r.head("足" + s); side = V3(0, (H - r.head("下半身")).y, 0)
        body.leg_ik(s, V3(H.x + 0.42, H.y + side.y * 0.6, 0.075), H + V3(0.6, side.y * 1.5, 0.2))
        body.foot(s, nrm(V3(1, side.y * 2, 0)), V3(0, 0, 1))

def sit_bed(ctx, t):
    V, M = ctx.V, ctx.M; r = V.rig; m = M.rig
    breath = 0.5 + 0.5 * math.sin(2 * math.pi * t)
    sit_edge(V, MAT_W / 2 - 0.20, 0.95, lean=6 + 2 * breath)
    sit_edge(M, MAT_W / 2 - 0.22, 1.45, lean=10)
    bvhV = surface(V); bvhM = surface(M)
    vs = dict(sides(V, "腕")); ms = dict(sides(M, "腕"))
    for s in (".L", ".R"):                                            # her hands rest on her thighs
        H = r.head("足" + s); K = r.head("ひざ" + s)
        cup(V, s, bvhV, H + (K - H) * 0.6 + V3(0, 0, 0.07), nrm(K - H), r.head("腕" + s) + V3(0.1, 0, -0.3))
    near = ".L" if (m.head("腕.L") - V.rig.head("上半身2")).length < (m.head("腕.R") - V.rig.head("上半身2")).length else ".R"
    far = ".R" if near == ".L" else ".L"
    cup(M, near, bvhV, r.head("上半身2") + V3(-0.08, 0.10, 0.08), nrm(V3(0, -1, 0.2)), m.head("腕" + near) + V3(-0.2, 0, -0.3))   # his arm around her
    H = m.head("足" + far); K = m.head("ひざ" + far)
    cup(M, far, bvhM, H + (K - H) * 0.55 + V3(0, 0, 0.08), nrm(K - H), m.head("腕" + far) + V3(0.1, 0.2, -0.3))
    V.look(m.head("頭") + V3(0.05, 0, -0.05)); M.look(r.head("頭"))

P.CLIPS.update({
    "stand.idle": (lambda c, t: stand_idle(c, t), 49, "loop"),
    "sit.bed":    (lambda c, t: sit_bed(c, t), 49, "loop"),
})
