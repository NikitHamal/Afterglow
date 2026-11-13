import bpy, sys, os
from mathutils import Vector

# Load .blend
bpy.ops.wm.open_mainfile(filepath=r"F:/Afterglow/assets/goatchan/goatchan.blend")

print("=== ARMATURE BONES ===")
arm = None
for o in bpy.data.objects:
    if o.type == 'ARMATURE':
        arm = o
        break
print("armature:", arm.name if arm else "NONE")
if arm:
    bones = arm.data.bones
    print(f"total bones: {len(bones)}")
    interesting = ["thigh先","ankle","thigh","knee","kneeL","thighL","ankleL","thigh先L",
                   "thighR","ankleR","thigh先R","wrist","wristL","wristR","middle1","index1",
                   "pinky1","thumb1","ring1","thighthumb","thighindex"]
    for b in bones:
        n = b.name
        if any(k in n for k in interesting):
            print(f"  {n}  parent={b.parent.name if b.parent else 'None'}  head={tuple(round(x,3) for x in b.head_local)} tail={tuple(round(x,3) for x in b.tail_local)}  len={round(b.length,3)}")

print("\n=== MESHES ===")
for o in bpy.data.objects:
    if o.type == 'MESH':
        print(f"  mesh: {o.name}  verts={len(o.data.vertices)}  polys={len(o.data.polygons)}")
        # mat names
        mats = list({p.material.name for p in o.data.polygons if p.material})
        print(f"    materials: {mats[:6]}{'...' if len(mats)>6 else ''}")
