# Textured anatomy close-up on the stage-4 body written by build5.py (legs apart, review views).
#   blender -b out/vivian_s4.blend --python test_vulva.py -- <tag>
import bpy, sys, math, os, shlex
import numpy as np
from mathutils import Vector, Matrix
sys.path.insert(0, "/root/agb")
OUT = "/root/agb/out"; tag = sys.argv[sys.argv.index("--") + 1] if "--" in sys.argv else "vt"
body = bpy.data.objects["Human"]; rig = bpy.data.objects["Human.rig"]
# legs apart + hips flexed (pose only, for the camera)
bpy.context.view_layer.objects.active = rig; bpy.ops.object.mode_set(mode="POSE")
for bn, sgn in (("thigh_l", -1), ("thigh_r", 1)):
    pb = rig.pose.bones[bn]; h = pb.head.copy()
    R = Matrix.Rotation(math.radians(38) * sgn, 4, "Y") @ Matrix.Rotation(math.radians(-55), 4, "X")
    pb.matrix = Matrix.Translation(h) @ R @ Matrix.Translation(-h) @ pb.matrix
bpy.ops.object.mode_set(mode="OBJECT")
sc = bpy.context.scene
sc.render.engine = "CYCLES"; sc.cycles.device = "CPU"; sc.cycles.samples = 48; sc.cycles.use_denoising = True
sc.view_settings.view_transform = "AgX"; sc.view_settings.exposure = -0.9
w = bpy.data.worlds.new("w"); sc.world = w; w.use_nodes = True; w.node_tree.nodes["Background"].inputs[0].default_value = (0.05, 0.04, 0.045, 1)
for o in bpy.data.objects:
    if o.type == "MESH" and o != body: o.hide_render = True
def light(name, loc, energy, target):
    L = bpy.data.lights.new(name, "AREA"); L.energy = energy; L.size = 0.4
    o = bpy.data.objects.new(name, L); sc.collection.objects.link(o); o.location = loc
    o.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
tgt = Vector((0, -0.07, 0.842))
light("k", tgt + Vector((-0.35, -0.45, -0.25)), 60, tgt); light("f", tgt + Vector((0.4, -0.35, -0.3)), 25, tgt)
cam = bpy.data.cameras.new("c"); co = bpy.data.objects.new("c", cam); sc.collection.objects.link(co); sc.camera = co; cam.lens = 70
paths = []
for nm, loc, t, res in [("front", tgt + Vector((0, -0.30, -0.16)), tgt, (900, 700)),
                        ("low", tgt + Vector((0.0, -0.10, -0.32)), tgt + Vector((0, 0.02, 0)), (900, 700))]:
    co.location = loc; co.rotation_euler = (Vector(t) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
    sc.render.resolution_x, sc.render.resolution_y = res
    p = f"{OUT}/{tag}_{nm}.png"; sc.render.filepath = p; bpy.ops.render.render(write_still=True); paths.append(p)
os.system(f"python3 /root/agb/sheet.py {OUT}/{tag}_sheet.jpg 500 " + " ".join(paths))
os.system("python3 /root/agb/progress.py frames " + shlex.quote(f"{OUT}/{tag}_sheet.jpg"))
os.system("python3 /root/agb/progress.py log " + shlex.quote("[F] anatomy: textured close-up (caramel tone)"))
