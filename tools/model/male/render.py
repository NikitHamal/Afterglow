# Textured check renders for Marcus (pushed to the dashboard). usage: -- <tag> [hair]
import bpy, math, os, json, shlex, sys
from mathutils import Vector
M = "/root/agb/male"; OUT = M + "/out"; tag = sys.argv[sys.argv.index("--") + 1]
L = json.load(open(OUT + "/landmarks.json")); EL, ER = Vector(L["EL"]), Vector(L["ER"]); G = json.load(open(OUT + "/genital.json")); B = Vector(G["B"])
sc = bpy.context.scene
sc.render.engine = "CYCLES"; sc.cycles.device = "CPU"; sc.cycles.samples = 48; sc.cycles.use_denoising = True; sc.cycles.transparent_max_bounces = 48
sc.view_settings.view_transform = "AgX"; sc.view_settings.look = "AgX - Medium High Contrast"; sc.view_settings.exposure = -0.8
w = bpy.data.worlds.new("w"); sc.world = w; w.use_nodes = True; w.node_tree.nodes["Background"].inputs[0].default_value = (0.05, 0.035, 0.04, 1)
meshes = [o for o in sc.objects if o.type == "MESH"]
zs = [(o.matrix_world @ Vector(c)).z for o in meshes for c in o.bound_box]; top, bot = max(zs), min(zs); h = top - bot; mid = (top + bot) / 2
def light(name, loc, energy, size, color, target):
    Ld = bpy.data.lights.new(name, "AREA"); Ld.energy = energy; Ld.size = size; Ld.color = color
    o = bpy.data.objects.new(name, Ld); sc.collection.objects.link(o); o.location = loc
    o.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
light("key", (-1.9, -2.3, mid + 1.3), 700, 1.6, (1.0, 0.85, 0.72), (0, 0, mid + 0.3))
light("fill", (2.4, -2.2, mid + 0.4), 140, 3.0, (0.86, 0.82, 1.0), (0, 0, mid))
light("rimL", (-1.6, 1.9, mid + 1.0), 480, 1.2, (1.0, 0.75, 0.62), (0, 0, mid + 0.2))
light("rimR", (1.7, 1.8, mid + 1.2), 560, 1.2, (0.72, 0.82, 1.0), (0, 0, mid + 0.2))
cam = bpy.data.cameras.new("c"); co = bpy.data.objects.new("c", cam); sc.collection.objects.link(co); sc.camera = co
paths = []
def shot(name, target, ang, elev, dist, lens, res):
    sc.render.resolution_x, sc.render.resolution_y = res; cam.lens = lens; a = math.radians(ang); target = Vector(target)
    co.location = target + Vector((math.sin(a) * dist, -math.cos(a) * dist, elev))
    co.rotation_euler = (target - co.location).to_track_quat("-Z", "Y").to_euler()
    p = f"{OUT}/{tag}_{name}.png"; sc.render.filepath = p; bpy.ops.render.render(write_still=True); paths.append(p)
    os.system("python3 /root/agb/progress.py frames " + shlex.quote(p))
head = (EL + ER) / 2
shot("face", head + Vector((0, 0.01, -0.035)), 22, 0.02, 0.8, 85, (700, 800))
shot("full", (0, 0, mid), 20, 0.15, h * 2.15, 60, (560, 800))
shot("back", (0, 0, mid), 180, 0.2, h * 2.15, 60, (560, 800))
shot("groin", B + Vector((0, -0.05, -0.02)), 35, 0.06, 0.62, 70, (640, 640))
os.system(f"python3 /root/agb/sheet.py {OUT}/{tag}_sheet.jpg 560 " + " ".join(paths))
# expressions
bodyo = [o for o in meshes if o.data.shape_keys and "blink" in o.data.shape_keys.key_blocks]
ep = []
sc.render.resolution_x, sc.render.resolution_y = 420, 480; cam.lens = 85
tgt = head + Vector((0, 0, -0.035)); co.location = tgt + Vector((math.sin(math.radians(14)) * 0.6, -math.cos(math.radians(14)) * 0.6, 0.01))
co.rotation_euler = (tgt - co.location).to_track_quat("-Z", "Y").to_euler()
for nm in ("neutral", "blink", "effort", "pleasure", "moan", "kiss"):
    for o in bodyo:
        for kb in o.data.shape_keys.key_blocks[1:]: kb.value = 1.0 if kb.name == nm else 0.0
    p = f"{OUT}/{tag}_x_{nm}.png"; sc.render.filepath = p; bpy.ops.render.render(write_still=True); ep.append(p)
os.system(f"python3 /root/agb/sheet.py {OUT}/{tag}_expr.jpg 420 " + " ".join(ep))
os.system("python3 /root/agb/progress.py frames " + shlex.quote(f"{OUT}/{tag}_expr.jpg"))
