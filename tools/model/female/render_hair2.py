# Hair check renders (Cycles, 4 views of the head) from the rigged real-scale model.
#   blender -b out/vivian_game.blend --python render_hair2.py -- <out_dir> [tone]
import bpy, sys, os, math
from mathutils import Vector
argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUTD = argv[0] if argv else "/root/agb/out/preview"; os.makedirs(OUTD, exist_ok=True)
sc = bpy.context.scene
sc.render.engine = "CYCLES"; sc.cycles.device = "CPU"; sc.cycles.samples = 96; sc.cycles.use_denoising = True
sc.render.resolution_x, sc.render.resolution_y = 900, 900
sc.view_settings.view_transform = "AgX"; sc.view_settings.look = "AgX - Medium High Contrast"
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
hb = arm.data.bones["頭"]; H = arm.matrix_world @ hb.head_local
C = H + Vector((0, 0.01, 0.07))
w = bpy.data.worlds.new("w"); w.use_nodes = True; w.node_tree.nodes["Background"].inputs[0].default_value = (0.02, 0.018, 0.02, 1); sc.world = w
def light(name, loc, power, color, size=0.6):
    ld = bpy.data.lights.new(name, "AREA"); ld.energy = power; ld.color = color; ld.size = size
    ob = bpy.data.objects.new(name, ld); sc.collection.objects.link(ob); ob.location = loc
    ob.rotation_euler = (C - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
light("key", C + Vector((-0.9, -1.0, 0.7)), 90, (1.0, 0.86, 0.72))
light("rim", C + Vector((0.8, 1.0, 0.6)), 70, (0.75, 0.82, 1.0))
light("fill", C + Vector((1.0, -0.8, -0.1)), 25, (1.0, 0.95, 0.9), 1.2)
cam = bpy.data.cameras.new("c"); cam.lens = 85; co = bpy.data.objects.new("c", cam); sc.collection.objects.link(co); sc.camera = co
for o in bpy.data.objects:                                  # neutral pose, no expression
    if o.type == "MESH" and o.data.shape_keys:
        for kb in o.data.shape_keys.key_blocks[1:]: kb.value = 0.0
VIEWS = (("front", (0, -1.0, 0.05)), ("three_q", (-0.72, -0.72, 0.12)), ("side", (-1.0, 0.05, 0.05)), ("back", (0.35, 1.0, 0.18)), ("top", (0.05, -0.35, 1.0)))
if len(argv) > 1: VIEWS = [v for v in VIEWS if v[0] in argv[1].split(",")]
for name, off in VIEWS:
    co.location = C + Vector(off) * 0.95
    co.rotation_euler = (C - co.location).to_track_quat("-Z", "Y").to_euler()
    sc.render.filepath = f"{OUTD}/hair_{name}.png"; bpy.ops.render.render(write_still=True)
    print("[hair-render]", name, flush=True)
