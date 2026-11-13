import bpy
from mathutils import Vector

bpy.ops.wm.open_mainfile(filepath=r"F:/Afterglow/assets/goatchan/goatchan.blend")

# Find all meshes and pick the body
print("=== ALL MESHES ===")
for o in bpy.data.objects:
    if o.type == 'MESH':
        print(f"  {o.name}  verts={len(o.data.vertices)}")

# Pick biggest mesh
meshes = [o for o in bpy.data.objects if o.type == 'MESH']
meshes.sort(key=lambda o: -len(o.data.vertices))
mesh_obj = meshes[0]
arm_obj = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
print(f"\nPicked body: {mesh_obj.name}  verts={len(mesh_obj.data.vertices)}")

m = mesh_obj.data
xs=[v.co.x for v in m.vertices]; ys=[v.co.y for v in m.vertices]; zs=[v.co.z for v in m.vertices]
print(f"BBox: x[{min(xs):.3f},{max(xs):.3f}] y[{min(ys):.3f},{max(ys):.3f}] z[{min(zs):.3f},{max(zs):.3f}]")

# Top bone per vertex
def top_bone(v):
    if not v.groups: return None
    g = max(v.groups, key=lambda g: g.weight)
    return (arm_obj.data.bones[g.group].name, round(g.weight,2))

buckets = {}
for v in m.vertices:
    b = top_bone(v)
    if b is None: continue
    n,_ = b
    buckets.setdefault(n, []).append(v.co.copy())

# Print focus
focus = ["wrist.L","wrist.R",
         "thumb0.L","thumb1.L","thumb2.L","index0.L","index1.L","index2.L",
         "middle0.L","middle1.L","middle2.L","ring0.L","ring1.L","ring2.L",
         "pinky0.L","pinky1.L","pinky2.L",
         "thumb0.R","thumb1.R","index1.R","middle1.R","ring1.R","pinky1.R",
         "ankle.L","ankle.R","thigh.L","thigh.R","knee.L","knee.R","pelvis",
         "spine","chest","thigh捩.L","thigh捩.R"]
print("\n=== DOMINANT BONE BBOX ===")
for n in focus:
    vs = buckets.get(n, [])
    if not vs: print(f"  {n}: NO verts"); continue
    xs=[v.x for v in vs]; ys=[v.y for v in vs]; zs=[v.z for v in vs]
    print(f"  {n}: n={len(vs):4d}  x[{min(xs):+.3f},{max(xs):+.3f}] y[{min(ys):+.3f},{max(ys):+.3f}] z[{min(zs):+.3f},{max(zs):+.3f}]")

# Check if there's a foot toe bone
print("\n=== ALL BONE NAMES containing thigh ===")
for b in arm_obj.data.bones:
    if "thigh" in b.name:
        print(f"  {b.name}  parent={b.parent.name if b.parent else 'None'}  len={b.length:.3f}")

# Vertex count weighted to each finger
print("\n=== FINGER VERT COUNTS ===")
for finger in ["thumb","index","middle","ring","pinky"]:
    for side in [".L",".R"]:
        for seg in ["0","1","2"]:
            n = f"{finger}{seg}{side}"
            print(f"  {n}: {len(buckets.get(n,[]))}")

# Vertex count for hand area (any finger bone)
hand_L = sum(len(buckets.get(f"{p}{s}{side}",[])) for p in ["thumb","index","middle","ring","pinky"] for s in ["0","1","2"] for side in [".L"])
hand_R = sum(len(buckets.get(f"{p}{s}{side}",[])) for p in ["thumb","index","middle","ring","pinky"] for s in ["0","1","2"] for side in [".R"])
print(f"\nTotal finger verts: L={hand_L} R={hand_R}")
print(f"Total wrist verts: L={len(buckets.get('wrist.L',[]))} R={len(buckets.get('wrist.R',[]))}")
print(f"Total ankle verts: L={len(buckets.get('ankle.L',[]))} R={len(buckets.get('ankle.R',[]))}")
print(f"Total leg verts: L={len(buckets.get('thigh.L',[]))} R={len(buckets.get('thigh.R',[]))}")
print(f"Total knee verts: L={len(buckets.get('knee.L',[]))} R={len(buckets.get('knee.R',[]))}")
