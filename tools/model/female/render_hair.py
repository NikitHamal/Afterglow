# Hair/body preview set: front, 3/4, side, back, face 3/4. Pushed to the molab dashboard.
import bpy, math, os, json, shlex, sys
from mathutils import Vector
OUT = "/root/agb/out"; TEX = OUT + "/tex"
tag = sys.argv[sys.argv.index("--") + 1] if "--" in sys.argv and len(sys.argv) > sys.argv.index("--") + 1 else "h"
L = json.load(open(OUT + "/landmarks.json")); EL, ER = Vector(L["EL"]), Vector(L["ER"])
sc = bpy.context.scene
sc.render.engine = "CYCLES"; sc.cycles.device = "CPU"; sc.cycles.samples = 64; sc.cycles.use_denoising = True
sc.cycles.transparent_max_bounces = 64; sc.cycles.max_bounces = 8
sc.view_settings.view_transform = "AgX"; sc.view_settings.look = "AgX - Medium High Contrast"; sc.view_settings.exposure = -0.9
world = bpy.data.worlds.new("w"); sc.world = world; world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.05, 0.035, 0.04, 1)
meshes = [o for o in sc.objects if o.type == "MESH" and o.visible_get()]
zs = [(o.matrix_world @ Vector(c)).z for o in meshes for c in o.bound_box]; top, bot = max(zs), min(zs); h = top - bot; mid = (top + bot) / 2
bpy.ops.mesh.primitive_plane_add(size=14, location=(0, 0, bot - 0.001)); fl = bpy.context.object
fm = bpy.data.materials.new("floor"); fm.use_nodes = True
fm.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.035, 0.02, 0.025, 1); fl.data.materials.append(fm)
def light(name, loc, energy, size, color, target):
    Ld = bpy.data.lights.new(name, "AREA"); Ld.energy = energy; Ld.size = size; Ld.color = color
    o = bpy.data.objects.new(name, Ld); sc.collection.objects.link(o); o.location = loc
    o.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
light("key", (-1.9, -2.3, mid + 1.3), 700, 1.6, (1.0, 0.85, 0.72), (0, 0, mid + 0.3))
light("fill", (2.4, -2.2, mid + 0.4), 140, 3.0, (0.86, 0.82, 1.0), (0, 0, mid))
light("rimL", (-1.6, 1.9, mid + 1.0), 480, 1.2, (1.0, 0.75, 0.62), (0, 0, mid + 0.2))
light("rimR", (1.7, 1.8, mid + 1.2), 560, 1.2, (0.72, 0.82, 1.0), (0, 0, mid + 0.2))
cam = bpy.data.cameras.new("cam"); camo = bpy.data.objects.new("cam", cam); sc.collection.objects.link(camo); sc.camera = camo
paths = []
def shot(name, target, ang, elev, dist, lens, res):
    sc.render.resolution_x, sc.render.resolution_y = res; cam.lens = lens; a = math.radians(ang); target = Vector(target)
    camo.location = target + Vector((math.sin(a) * dist, -math.cos(a) * dist, elev))
    camo.rotation_euler = (target - camo.location).to_track_quat("-Z", "Y").to_euler()
    p = f"{OUT}/{tag}_{name}.png"; sc.render.filepath = p; bpy.ops.render.render(write_still=True); paths.append(p)
    os.system("python3 /root/agb/progress.py frames " + shlex.quote(p))
head = (EL + ER) / 2
upper = Vector((0, 0.02, head.z - 0.22))
shot("face", head + Vector((0, 0.01, -0.04)), 25, 0.03, 0.75, 85, (760, 860))
shot("bust", upper, 20, 0.08, 1.35, 70, (760, 860))
shot("side", upper, 90, 0.05, 1.45, 70, (760, 860))
shot("back", upper, 180, 0.12, 1.45, 70, (760, 860))
shot("full", (0, 0, mid), 25, 0.15, h * 2.15, 60, (620, 860))
os.system(f"python3 /root/agb/sheet.py {OUT}/{tag}_sheet.jpg 560 " + " ".join(paths))
