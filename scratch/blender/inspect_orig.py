import bpy
bpy.ops.import_scene.gltf(filepath=r"F:/Afterglow/assets/goatchan/goatchan.glb")
for o in bpy.data.objects:
    if o.type == 'ARMATURE':
        print(f"Armature: {o.name} bones={len(o.data.bones)}")
        for b in sorted(o.data.bones, key=lambda x: x.name):
            if any(k in b.name for k in ["足","つま","腕","ひじ","手首","親指","人指","中指","薬指","小指","上半身","下半身","首","頭"]):
                print(f"  {b.name!r}")
        break
