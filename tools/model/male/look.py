# textured look check of the final Marcus (stage D): face 3/4, full body (soft), groin soft vs aroused -> dashboard
#   blender -b out/marcus_d.blend --python male_look.py -- <tag>
import bpy, math, os, json, shlex, sys
from mathutils import Vector
M = "/root/agb/male"; OUT = M + "/out"; tag = sys.argv[sys.argv.index("--") + 1]
G = json.load(open(OUT + "/genital.json")); B = Vector(G["B"])
L = json.load(open(OUT + "/landmarks.json")); EC = (Vector(L["EL"]) + Vector(L["ER"])) / 2
sc = bpy.context.scene
sc.render.engine = "CYCLES"; sc.cycles.device = "CPU"; sc.cycles.samples = 40; sc.cycles.use_denoising = True
sc.cycles.transparent_max_bounces = 48
sc.view_settings.view_transform = "AgX"; sc.view_settings.look = "AgX - Medium High Contrast"; sc.view_settings.exposure = -0.7
w = bpy.data.worlds.new("w"); sc.world = w; w.use_nodes = True; w.node_tree.nodes["Background"].inputs[0].default_value = (0.05, 0.035, 0.04, 1)
body = bpy.data.objects["Human"]
kb = body.data.shape_keys.key_blocks
for k in kb: k.value = 0
meshes = [o for o in sc.objects if o.type == "MESH"]
zs = [(o.matrix_world @ Vector(c)).z for o in meshes for c in o.bound_box]; top, bot = max(zs), min(zs); mid = (top + bot) / 2
def light(name, loc, energy, size, color, target):
    Ld = bpy.data.lights.new(name, "AREA"); Ld.energy = energy; Ld.size = size; Ld.color = color
    o = bpy.data.objects.new(name, Ld); sc.collection.objects.link(o); o.location = loc
    o.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
light("key", (-1.9, -2.3, mid + 1.3), 760, 1.6, (1.0, 0.85, 0.72), (0, 0, mid + 0.3))
light("fill", (2.4, -2.2, mid + 0.4), 150, 3.0, (0.86, 0.82, 1.0), (0, 0, mid))
light("rim", (1.7, 1.8, mid + 1.2), 560, 1.2, (0.72, 0.82, 1.0), (0, 0, mid + 0.2))
cam = bpy.data.cameras.new("c"); co = bpy.data.objects.new("c", cam); sc.collection.objects.link(co); sc.camera = co
paths = []
def shot(name, tgt, ang, elev, dist, lens, res):
    sc.render.resolution_x, sc.render.resolution_y = res; cam.lens = lens; a = math.radians(ang); tgt = Vector(tgt)
    co.location = tgt + Vector((math.sin(a) * dist, -math.cos(a) * dist, elev))
    co.rotation_euler = (tgt - co.location).to_track_quat("-Z", "Y").to_euler()
    p = f"{OUT}/{tag}_{name}.png"; sc.render.filepath = p; bpy.ops.render.render(write_still=True); paths.append(p)
    os.system("python3 /root/agb/progress.py frames " + shlex.quote(p))
shot("face", EC + Vector((0, 0.01, -0.03)), 28, 0.03, 0.8, 85, (620, 720))
shot("full", (0, 0, mid), 20, 0.15, (top - bot) * 2.2, 60, (520, 720))
g = B + Vector((0, -0.04, -0.05))
shot("soft", g, 32, 0.06, 0.62, 70, (560, 600))
kb["aroused"].value = 1.0
shot("hard", g, 32, 0.06, 0.62, 70, (560, 600))
os.system(f"python3 /root/agb/sheet.py {OUT}/{tag}_sheet.jpg 560 " + " ".join(paths))
