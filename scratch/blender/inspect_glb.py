import bpy
bpy.ops.import_scene.gltf(filepath=r"F:/Afterglow/assets/goatchan/goatchan.glb")
for o in bpy.data.objects:
    if o.type == 'ARMATURE':
        print(f"Armature: {o.name}")
        bones = sorted(b.name for b in o.data.bones)
        for b in bones:
            if "足" in b or "つま" in b or "腕" in b or "ひじ" in b or "手首" in b or "親" in b or "上半身" in b or "下半身" in b:
                print(f"  {repr(b)}")
        break
