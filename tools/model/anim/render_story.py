# QA renders of baked clips from anim.blend, framed on the heads (story kisses / embraces): Cycles, small, fast.
#   blender -b <AG_NAT>/anim.blend --python render_story.py -- <out_dir> clip[:t,...] [clip...] [--views side,front,top,back] [--frame heads|upper]
# Writes <out_dir>/<clip>_<t>_<view>.png. Bodies are framed head-and-shoulders ('heads') or to the waist ('upper').
import bpy, sys, os, math
from mathutils import Vector
argv = sys.argv[sys.argv.index("--") + 1:]
out = argv[0]; os.makedirs(out, exist_ok=True)
views = ["side", "front", "top", "back"]; framing = "heads"
items = []
i = 1
while i < len(argv):
    if argv[i] == "--views": views = argv[i + 1].split(","); i += 2; continue
    if argv[i] == "--frame": framing = argv[i + 1]; i += 2; continue
    items.append(argv[i]); i += 1
sc = bpy.context.scene
sc.render.engine = "CYCLES"; sc.cycles.device = "CPU"; sc.cycles.samples = 16; sc.cycles.use_denoising = True
sc.render.resolution_x, sc.render.resolution_y = 720, 540
if sc.world is None: sc.world = bpy.data.worlds.new("qa")
sc.world.use_nodes = True
bg = sc.world.node_tree.nodes["Background"]; bg.inputs[0].default_value = (0.55, 0.55, 0.58, 1); bg.inputs[1].default_value = 1.2
for o in sc.objects:                                   # the room stays out of the frame's way (only bodies + hair)
    if o.type == "MESH" and not o.name.startswith(("vivian_", "marcus_")): o.hide_render = True
    if o.type == "LIGHT": o.hide_render = True
cam = bpy.data.cameras.new("qa"); cam.lens = 50
co = bpy.data.objects.new("qa_cam", cam); sc.collection.objects.link(co); sc.camera = co
sun = bpy.data.lights.new("qa_key", "SUN"); sun.energy = 2.5
so = bpy.data.objects.new("qa_key", sun); sc.collection.objects.link(so); so.rotation_euler = (math.radians(50), 0, math.radians(30))
V = bpy.data.objects["VivianRig"]; M = bpy.data.objects["MarcusRig"]
for it in items:
    clip, _, ts = it.partition(":")
    ts = [float(x) for x in ts.split(",")] if ts else [0.0]
    av, am = bpy.data.actions.get("VIV|" + clip), bpy.data.actions.get("MAR|" + clip)
    if av is None or am is None: print("[qa] no clip", clip); continue
    V.animation_data_create(); M.animation_data_create(); V.animation_data.action = av; M.animation_data.action = am
    f0, f1 = av.frame_range
    for t in ts:
        sc.frame_set(int(round(f0 + (f1 - f0) * t))); bpy.context.view_layer.update()
        hv = V.matrix_world @ V.pose.bones["頭"].head; hm = M.matrix_world @ M.pose.bones["頭"].head
        cv = V.matrix_world @ V.pose.bones["上半身2"].head; cm = M.matrix_world @ M.pose.bones["上半身2"].head
        pv_ = V.matrix_world @ V.pose.bones["下半身"].head; pm_ = M.matrix_world @ M.pose.bones["下半身"].head
        tgt = (hv + hm) / 2 if framing == "heads" else ((hv + hm + cv + cm) / 4 if framing == "upper" else (pv_ + pm_) / 2)
        ax = (hm - hv); ax.z = 0; ax.normalize(); side = Vector((0, 0, 1)).cross(ax).normalized()
        fwd_v = (V.matrix_world.to_3x3() @ V.pose.bones["上半身2"].matrix.to_3x3() @ Vector((0, -1, 0))) if False else None
        facing = (V.matrix_world @ V.pose.bones["頭"].tail) - hv + (M.matrix_world @ M.pose.bones["頭"].tail) - hm
        bodies_fwd = ((V.matrix_world @ V.pose.bones["上半身2"].head) - (V.matrix_world @ V.pose.bones["下半身"].head))
        nose = sum(((V.matrix_world @ V.pose.bones[n].head) for n in ("目.L", "目.R") if n in V.pose.bones), Vector()) / 2 - hv
        if nose.length > 1e-6 and nose.dot(side) + (sum(((M.matrix_world @ M.pose.bones[n].head) for n in ("目.L", "目.R") if n in M.pose.bones), Vector()) / 2 - hm).dot(side) < 0:
            side = -side                                   # look from the side the faces are on
        dist = 0.75 if framing == "heads" else (1.6 if framing == "upper" else 1.9)
        eyes = {"side": tgt + side * dist + Vector((0, 0, 0.05)), "back": tgt - side * dist + Vector((0, 0, 0.05)),
                "front": tgt - ax * dist * 0.7 + side * dist * 0.7 + Vector((0, 0, 0.12)), "top": tgt + Vector((0, 0, dist)) + side * 0.15}
        for vname in views:
            e = eyes[vname]; co.location = e
            co.rotation_euler = (tgt - e).to_track_quat("-Z", "Y").to_euler()
            sc.render.filepath = os.path.join(out, f"{clip.replace('.', '_')}_{int(t * 100):03d}_{vname}.png")
            bpy.ops.render.render(write_still=True)
            print("[qa] wrote", sc.render.filepath, flush=True)
