import bpy
bpy.ops.import_scene.gltf(filepath=r"F:/Afterglow/assets/goatchan/goatchan.glb")
for o in bpy.data.objects:
    if o.type == 'ARMATURE':
        knees = [b.name for b in o.data.bones if "ひざ" in b.name or "膝" in b.name]
        print(f"KNEE BONES: {knees}")
        print(f"TOTAL: {len(o.data.bones)}")
        # also check meshes
        break
