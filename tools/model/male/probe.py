# Probe: MPFB male base mesh — genital geometry + overall proportions.
import bpy, os, json
import numpy as np
from bl_ext.user_default.mpfb.services.humanservice import HumanService
from bl_ext.user_default.mpfb.services.targetservice import TargetService
bpy.ops.wm.read_factory_settings(use_empty=True)
info = HumanService._create_default_human_info_dict()
info["phenotype"] = {"gender": 1.0, "age": 0.62, "muscle": 0.68, "weight": 0.55, "proportions": 0.85, "height": 0.62,
                     "cupsize": 0.5, "firmness": 0.5, "race": {"caucasian": 0.6, "african": 0.25, "asian": 0.15}}
s = HumanService.get_default_deserialization_settings(); s["subdiv_levels"] = 0
body = HumanService.deserialize_from_dict(info, s)
rig = HumanService.add_builtin_rig(body, "game_engine_with_breast", import_weights=True)
TargetService.bake_targets(body)
V = np.array([v.co for v in body.data.vertices])
def vg(name):
    gi = body.vertex_groups[name].index; w = np.zeros(len(V))
    for v in body.data.vertices:
        for g in v.groups:
            if g.group == gi: w[v.index] = g.weight
    return w
wb, wg = vg("body"), vg("genitals")
wh = vg("helper-genital")
print("PROBE counts genitals", int((wg > 0.5).sum()), "genitals&body", int(((wg > 0.5) & (wb > 0.5)).sum()), "helper-genital", int((wh > 0.5).sum()), "helper&body", int(((wh > 0.5) & (wb > 0.5)).sum()))
for nm in ("genitals", "helper-genital"):
    w_ = vg(nm); P_ = V[w_ > 0.5]
    if len(P_): print("PROBE", nm, "bbox", P_.min(0).round(3).tolist(), P_.max(0).round(3).tolist())
G = V[(wg > 0.5)] if (wg > 0.5).any() else V[(wh > 0.5)]
print("PROBE genital verts", len(G), "bbox", G.min(0).round(3).tolist(), G.max(0).round(3).tolist())
faces = [p for p in body.data.polygons if all(wg[i] > 0.5 for i in p.vertices)]
print("PROBE genital faces", len(faces))
J = {b.name: [round(x, 3) for x in (rig.matrix_world @ b.head_local)] for b in rig.data.bones}
print("PROBE joints", json.dumps({k: J[k] for k in ["pelvis", "spine_01", "neck_01", "head", "thigh_l", "calf_l", "foot_l", "upperarm_l", "lowerarm_l", "hand_l", "clavicle_l"]}))
print("PROBE height", round(V[wb > 0.5][:, 2].max(), 3))
# render groin close-up
sc = bpy.context.scene; sc.render.engine = "CYCLES"; sc.cycles.samples = 24; sc.cycles.device = "CPU"
sc.render.resolution_x, sc.render.resolution_y = 600, 600
w = bpy.data.worlds.new("w"); sc.world = w; w.use_nodes = True; w.node_tree.nodes["Background"].inputs[0].default_value = (0.5, 0.48, 0.46, 1)
from mathutils import Vector
c = Vector(G.mean(0))
cam = bpy.data.cameras.new("c"); co = bpy.data.objects.new("c", cam); sc.collection.objects.link(co); sc.camera = co; cam.lens = 60
for tag, off in (("front", Vector((0.15, -0.55, 0.05))), ("side", Vector((0.55, -0.1, 0.03)))):
    co.location = c + off; co.rotation_euler = (c - co.location).to_track_quat("-Z", "Y").to_euler()
    sc.render.filepath = f"/root/agb/male/out/probe_{tag}.png"; bpy.ops.render.render(write_still=True)
