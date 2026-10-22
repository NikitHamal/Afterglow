# Same lighting, three skin tones: face 3/4 + full figure for each, pushed to the molab dashboard.
import bpy, math, os, json, shlex, sys
from mathutils import Vector
OUT = "/root/agb/out"; TEX = OUT + "/tex"
L = json.load(open(OUT + "/landmarks.json")); EL, ER = Vector(L["EL"]), Vector(L["ER"])
sc = bpy.context.scene
sc.render.engine = "CYCLES"; sc.cycles.device = "CPU"; sc.cycles.samples = 64; sc.cycles.use_denoising = True
sc.view_settings.view_transform = "AgX"; sc.view_settings.look = "AgX - Medium High Contrast"; sc.view_settings.exposure = -0.3
world = bpy.data.worlds.new("w"); sc.world = world; world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.045, 0.03, 0.035, 1)
meshes = [o for o in sc.objects if o.type == "MESH" and o.visible_get()]
zs = [(o.matrix_world @ Vector(c)).z for o in meshes for c in o.bound_box]; top, bot = max(zs), min(zs); h = top - bot; mid = (top + bot) / 2
bpy.ops.mesh.primitive_plane_add(size=14, location=(0, 0, bot - 0.001)); fl = bpy.context.object
fm = bpy.data.materials.new("floor"); fm.use_nodes = True; fm.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.06, 0.035, 0.04, 1)
fl.data.materials.append(fm)
def light(name, loc, energy, size, color, target):
    Ld = bpy.data.lights.new(name, "AREA"); Ld.energy = energy; Ld.size = size; Ld.color = color
    o = bpy.data.objects.new(name, Ld); sc.collection.objects.link(o); o.location = loc
    o.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
light("key", (-1.9, -2.3, mid + 1.3), 780, 1.6, (1.0, 0.86, 0.74), (0, 0, mid + 0.3))
light("fill", (2.4, -2.2, mid + 0.4), 150, 3.0, (0.86, 0.82, 1.0), (0, 0, mid))
light("rimL", (-1.6, 1.9, mid + 1.0), 480, 1.2, (1.0, 0.75, 0.62), (0, 0, mid + 0.2))
light("rimR", (1.7, 1.8, mid + 1.2), 560, 1.2, (0.72, 0.82, 1.0), (0, 0, mid + 0.2))
cam = bpy.data.cameras.new("cam"); camo = bpy.data.objects.new("cam", cam); sc.collection.objects.link(camo); sc.camera = camo
def shot(path, target, ang, elev, dist, lens, res):
    sc.render.resolution_x, sc.render.resolution_y = res; cam.lens = lens; a = math.radians(ang); target = Vector(target)
    camo.location = target + Vector((math.sin(a) * dist, -math.cos(a) * dist, elev))
    camo.rotation_euler = (target - camo.location).to_track_quat("-Z", "Y").to_euler()
    sc.render.filepath = path; bpy.ops.render.render(write_still=True)
    os.system("python3 /root/agb/progress.py frames " + shlex.quote(path))
skin = bpy.data.materials["vivian_skin"].node_tree.nodes["SKIN_BASE"]
face_c = (EL + ER) / 2 + Vector((0, 0, -0.03))
paths = []
for tone in sys.argv[sys.argv.index("--") + 1:] or ["honey", "caramel", "deep"]:
    skin.image = bpy.data.images.load(f"{TEX}/vivian_skin_{tone}.jpg")
    p1 = f"{OUT}/tone_{tone}_face.png"; shot(p1, face_c, 28, 0.02, 0.6, 85, (720, 820))
    p2 = f"{OUT}/tone_{tone}_body.png"; shot(p2, (0, 0, mid), 18, 0.1, h * 2.2, 60, (600, 820))
    paths += [p1, p2]
os.system(f"python3 /root/agb/sheet.py {OUT}/tones_sheet.jpg 560 " + " ".join(paths))
