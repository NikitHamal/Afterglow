# Export the native character GLBs with their baked clips from anim.blend.
#   blender -b /root/agb/native/anim.blend --python export_chars.py -- [vivian,marcus]
# -> /root/agb/native/export/<who>_raw.glb  (post.mjs then fixes materials/textures/names -> <who>.glb)
import bpy, sys, os, json
argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
WHO = argv[0].split(",") if argv else ["vivian", "marcus"]
EXP = "/root/agb/native/export"; os.makedirs(EXP, exist_ok=True)
PFX = {"vivian": ("VivianRig", "VIV|"), "marcus": ("MarcusRig", "MAR|")}
def log(*a): print("[export]", *a, flush=True)
sc = bpy.context.scene
for who in WHO:
    rig_name, pfx = PFX[who]
    arm = bpy.data.objects[rig_name]
    objs = [arm] + [o for o in bpy.data.objects if o.parent == arm and o.type == "MESH"]
    for ob in bpy.data.objects: ob.select_set(False)
    for ob in objs: ob.select_set(True)
    bpy.context.view_layer.objects.active = arm
    # every clip action of this character becomes its own NLA track (the exporter writes one glTF animation each)
    ad = arm.animation_data or arm.animation_data_create()
    for tr in list(ad.nla_tracks): ad.nla_tracks.remove(tr)
    ad.action = None
    acts = sorted([a for a in bpy.data.actions if a.name.startswith(pfx)], key=lambda a: a.name)
    for a in acts:
        tr = ad.nla_tracks.new(); tr.name = a.name
        st = tr.strips.new(a.name, int(a.frame_range[0]), a); st.name = a.name
        tr.mute = False
    for ob in objs:                                    # expression/body morphs rest at 0 in the file
        sk = ob.data.shape_keys if ob.type == "MESH" else None
        if sk:
            for kb in sk.key_blocks[1:]: kb.value = 0.0
    for pb in arm.pose.bones: pb.matrix_basis.identity()
    out = f"{EXP}/{who}_raw.glb"
    bpy.ops.export_scene.gltf(filepath=out, export_format="GLB", use_selection=True, export_yup=True, export_apply=False,
                              export_skins=True, export_morph=True, export_morph_normal=True, export_try_sparse_sk=True,
                              export_animations=True, export_animation_mode="NLA_TRACKS", export_nla_strips=True,
                              export_force_sampling=False, export_anim_slide_to_zero=True, export_reset_pose_bones=True,
                              export_def_bones=False, export_rest_position_armature=True, export_extras=True,
                              export_image_format="AUTO", export_texcoords=True, export_normals=True, export_tangents=False,
                              export_materials="EXPORT", export_cameras=False, export_lights=False)
    log(f"{who}: {len(acts)} clips -> {out} ({os.path.getsize(out)/1e6:.1f} MB)")
    for tr in list(ad.nla_tracks): ad.nla_tracks.remove(tr)
