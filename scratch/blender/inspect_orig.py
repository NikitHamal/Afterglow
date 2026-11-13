import bpy
bpy.ops.import_scene.gltf(filepath=r"F:/Afterglow/assets/goatchan/goatchan.glb")
for o in bpy.data.objects:
    if o.type == 'ARMATURE':
        print(f"Armature: {o.name} bones={len(o.data.bones)}")
        for b in sorted(o.data.bones, key=lambda x: x.name):
            if any(k in b.name for k in ["thigh","つま","upper_arm","elbow","wrist","thumb","index","middle","ring","pinky","spine","pelvis","neck","head"]):
                print(f"  {b.name!r}")
        break
