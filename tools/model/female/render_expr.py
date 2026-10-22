# Face close-ups of each expression morph (applied to body + parts), pushed to the molab dashboard.
import bpy, math, os, json, shlex, sys
from mathutils import Vector
OUT = "/root/agb/out"
L = json.load(open(OUT + "/landmarks.json"))
EL, ER = Vector(L["EL"]), Vector(L["ER"])
sc = bpy.context.scene
sc.render.engine = "CYCLES"; sc.cycles.device = "CPU"; sc.cycles.samples = 40; sc.cycles.use_denoising = True
sc.view_settings.view_transform = "AgX"; sc.view_settings.look = "AgX - Medium High Contrast"; sc.view_settings.exposure = -0.45
world = bpy.data.worlds.new("w"); sc.world = world; world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.03, 0.02, 0.025, 1)
def light(name, loc, energy, size, color, target):
    Ld = bpy.data.lights.new(name, "AREA"); Ld.energy = energy; Ld.size = size; Ld.color = color
    o = bpy.data.objects.new(name, Ld); sc.collection.objects.link(o); o.location = loc
    o.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
head = (EL + ER) / 2
light("key", head + Vector((-0.9, -1.1, 0.5)), 160, 0.8, (1.0, 0.85, 0.72), head)
light("fill", head + Vector((1.0, -1.0, 0.0)), 45, 1.2, (0.85, 0.85, 1.0), head)
light("rim", head + Vector((0.6, 0.9, 0.4)), 120, 0.6, (1.0, 0.9, 0.85), head)
cam = bpy.data.cameras.new("cam"); camo = bpy.data.objects.new("cam", cam); sc.collection.objects.link(camo); sc.camera = camo
cam.lens = 85; sc.render.resolution_x, sc.render.resolution_y = 560, 640
tgt = head + Vector((0, 0, -0.035))
camo.location = tgt + Vector((math.sin(math.radians(14)) * 0.55, -math.cos(math.radians(14)) * 0.55, 0.01))
camo.rotation_euler = (tgt - camo.location).to_track_quat("-Z", "Y").to_euler()
meshes = [o for o in sc.objects if o.type == "MESH" and o.data.shape_keys]
names = ["neutral", "blink", "moan", "pleasure", "oral", "kiss", "smile"]
paths = []
for nm in names:
    for o in meshes:
        for kb in o.data.shape_keys.key_blocks[1:]:
            kb.value = 1.0 if kb.name == nm else 0.0
    p = f"{OUT}/expr_{nm}.png"; sc.render.filepath = p; bpy.ops.render.render(write_still=True); paths.append(p)
    os.system("python3 /root/agb/progress.py frames " + shlex.quote(p))
os.system("python3 /root/agb/sheet.py " + OUT + "/expr_sheet.jpg 520 " + " ".join(paths))
