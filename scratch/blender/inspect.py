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
    interesting = ["足先","足首","足","ひざ","ひざL","足L","足首L","足先L",
                   "足R","足首R","足先R","手首","手首L","手首R","中指１","人指１",
                   "小指１","親指１","薬指１","足親指","足人指"]
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
