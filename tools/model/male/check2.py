# anatomy check: flaccid / half / erect side + 3/4 views with neutral skin, pushed to the dashboard
#   blender -b out/marcus_b.blend --python male_check2.py -- <tag>
import bpy, math, os, json, shlex, sys
from mathutils import Vector
M = "/root/agb/male"; OUT = M + "/out"; tag = sys.argv[sys.argv.index("--") + 1]
G = json.load(open(OUT + "/genital.json")); B = Vector(G["B"])
sc = bpy.context.scene
sc.render.engine = "CYCLES"; sc.cycles.device = "CPU"; sc.cycles.samples = 28; sc.cycles.use_denoising = True
sc.view_settings.view_transform = "AgX"; sc.view_settings.exposure = -0.6
w = bpy.data.worlds.new("w"); sc.world = w; w.use_nodes = True; w.node_tree.nodes["Background"].inputs[0].default_value = (0.35, 0.33, 0.32, 1)
skin = bpy.data.materials.new("chk"); skin.use_nodes = True
bs = skin.node_tree.nodes["Principled BSDF"]; bs.inputs["Base Color"].default_value = (0.42, 0.25, 0.17, 1); bs.inputs["Roughness"].default_value = 0.5
body = bpy.data.objects["Human"]
for i in range(len(body.material_slots)): body.material_slots[i].material = skin
def light(name, loc, energy, size):
    Ld = bpy.data.lights.new(name, "AREA"); Ld.energy = energy; Ld.size = size
    o = bpy.data.objects.new(name, Ld); sc.collection.objects.link(o); o.location = loc
    o.rotation_euler = (B - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
light("k", B + Vector((-0.8, -1.0, 0.6)), 120, 1.0); light("f", B + Vector((0.9, -0.8, 0.1)), 50, 1.5); light("r", B + Vector((0.3, 1.0, 0.5)), 60, 1.0)
cam = bpy.data.cameras.new("c"); co = bpy.data.objects.new("c", cam); sc.collection.objects.link(co); sc.camera = co
paths = []
kb = body.data.shape_keys.key_blocks
def shot(name, off, tgt, lens=60, res=(460, 520)):
    sc.render.resolution_x, sc.render.resolution_y = res; cam.lens = lens
    co.location = tgt + off; co.rotation_euler = (tgt - co.location).to_track_quat("-Z", "Y").to_euler()
    p = f"{OUT}/{tag}_{name}.png"; sc.render.filepath = p; bpy.ops.render.render(write_still=True); paths.append(p)
    os.system("python3 /root/agb/progress.py frames " + shlex.quote(p))
c = B + Vector((0, -0.05, -0.04))
for a in (0.0, 0.5, 1.0):
    kb["aroused"].value = a
    shot(f"side{int(a*10)}", Vector((0.55, -0.08, 0.03)), c)
    shot(f"q34_{int(a*10)}", Vector((0.30, -0.46, 0.10)), c)
os.system(f"python3 /root/agb/sheet.py {OUT}/{tag}_sheet.jpg 460 " + " ".join(paths))
