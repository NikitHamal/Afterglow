import bpy
bpy.ops.import_scene.gltf(filepath=r"F:/Afterglow/assets/goatchan/goatchan.glb")
for o in bpy.data.objects:
    if o.type == 'ARMATURE':
        print(f"Armature: {o.name}")
        bones = sorted(b.name for b in o.data.bones)
        for b in bones:
            if "thigh" in b or "つま" in b or "upper_arm" in b or "elbow" in b or "wrist" in b or "親" in b or "spine" in b or "pelvis" in b:
                print(f"  {repr(b)}")
        break
