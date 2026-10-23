# Close-up renders of hand contacts from anim.blend (bright neutral light), for frame-by-frame visual checks.
#   blender -b /root/agb/native/anim.blend --python render_hands.py -- <out_dir> clip:frame:char:bone,...
import bpy, sys, os, math
from mathutils import Vector
argv = sys.argv[sys.argv.index("--") + 1:]
OUT = argv[0]; os.makedirs(OUT, exist_ok=True)
jobs = [j.split(":") for j in argv[1].split(",")]
sc = bpy.context.scene
sc.render.engine = "CYCLES"; sc.cycles.device = "CPU"; sc.cycles.samples = 24; sc.cycles.use_denoising = True
sc.render.resolution_x, sc.render.resolution_y = 640, 480
sc.view_settings.view_transform = "AgX"
for o in list(bpy.data.objects):                       # the room's own lights off, bright soft studio light on
    if o.type == "LIGHT": o.hide_render = True
for nm, loc, pw in (("k1", (1.5, -0.5, 2.6), 900), ("k2", (-1.5, 2.5, 2.4), 600), ("k3", (0.0, -1.5, 1.2), 400)):
    ld = bpy.data.lights.new(nm, "AREA"); ld.energy = pw; ld.size = 1.5
    ob = bpy.data.objects.new(nm, ld); sc.collection.objects.link(ob); ob.location = loc
    ob.rotation_euler = (Vector((0, 1.2, 0.7)) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
cam = bpy.data.cameras.new("hc"); cam.lens = 40; co = bpy.data.objects.new("hc", cam); sc.collection.objects.link(co); sc.camera = co
rigs = {"V": bpy.data.objects["VivianRig"], "M": bpy.data.objects["MarcusRig"]}
for clip, frame, ch, bone in jobs:
    for k, rg in rigs.items():
        rg.animation_data_create(); rg.animation_data.action = bpy.data.actions.get(("VIV|" if k == "V" else "MAR|") + clip)
    sc.frame_set(int(frame)); bpy.context.view_layer.update()
    rg = rigs[ch]; pb = rg.pose.bones[bone]
    tgt = rg.matrix_world @ pb.head
    for i, off in enumerate(((0.55, -0.35, 0.30), (-0.45, -0.45, 0.35))):
        co.location = tgt + Vector(off); co.rotation_euler = (tgt - co.location).to_track_quat("-Z", "Y").to_euler()
        sc.render.filepath = f"{OUT}/{clip.replace('.', '_')}_{frame}_{bone.replace('.', '')}_{i}.png"
        bpy.ops.render.render(write_still=True)
    print("[hands-render]", clip, frame, bone, flush=True)
