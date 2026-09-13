import bpy
bpy.ops.wm.open_mainfile(filepath=r"F:/Afterglow/assets/goatchan/goatchan.blend")
arm = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
for b in arm.data.bones:
    if "足" in b.name and len(b.name) <= 6:
        # Show hex of each char in name
        print(f"  {repr(b.name)}  bytes={b.name.encode('utf-8')!r}")
