import bpy
from mathutils import Vector

bpy.ops.wm.open_mainfile(filepath=r"F:/Afterglow/assets/goatchan/goatchan.blend")

# Find mesh
mesh_obj = None; arm_obj = None
for o in bpy.data.objects:
    if o.type == 'MESH': mesh_obj = o
    if o.type == 'ARMATURE': arm_obj = o

m = mesh_obj.data
print(f"Mesh: {mesh_obj.name}  verts={len(m.vertices)}  polys={len(m.polygons)}")

# Compute bounding box
xs=[v.co.x for v in m.vertices]; ys=[v.co.y for v in m.vertices]; zs=[v.co.z for v in m.vertices]
print(f"BBox: x[{min(xs):.3f},{max(xs):.3f}] y[{min(ys):.3f},{max(ys):.3f}] z[{min(zs):.3f},{max(zs):.3f}]")

# Helper: dominant weight per vertex
def top_bone(v):
    if not v.groups: return None
    g = max(v.groups, key=lambda g: g.weight)
    return (arm_obj.data.bones[g.group].name, round(g.weight,2))

# Classify vertices by dominant bone and report bbox of those
buckets = {}
for v in m.vertices:
    b = top_bone(v)
    if b is None: continue
    n,_ = b
    buckets.setdefault(n, []).append(v.co.copy())

# Focus on hand/foot bones
focus = ["wrist.L","wrist.R","thumb0.L","thumb1.L","middle1.L","index1.L","pinky1.L","ring1.L",
         "ankle.L","ankle.R","thigh.L","thigh.R","knee.L","knee.R","pelvis","spine","chest"]
for n in focus:
    vs = buckets.get(n, [])
    if not vs: print(f"  {n}: NO verts"); continue
    xs=[v.x for v in vs]; ys=[v.y for v in vs]; zs=[v.z for v in vs]
    print(f"  {n}: n={len(vs):4d}  x[{min(xs):+.3f},{max(xs):+.3f}] y[{min(ys):+.3f},{max(ys):+.3f}] z[{min(zs):+.3f},{max(zs):+.3f}]")

# What % of hand-area vertices go to fingers vs wrist?
print("\n--- Finger area verts ---")
for prefix in ["thumb","index","middle","ring","pinky"]:
    c = sum(len(buckets.get(f"{prefix}0.L",[])) + len(buckets.get(f"{prefix}1.L",[])) + len(buckets.get(f"{prefix}2.L",[])) +
            len(buckets.get(f"{prefix}0.R",[])) + len(buckets.get(f"{prefix}1.R",[])) + len(buckets.get(f"{prefix}2.R",[])) for _ in [0])
print(f"Finger total verts: {c}")
print(f"Wrist L+R: {len(buckets.get('wrist.L',[])) + len(buckets.get('wrist.R',[]))}")

# Are there foot-toe bones that are NOT being used?
print("\n--- Bones with 0 vertices weighted ---")
used = set(buckets.keys())
all_bones = {b.name for b in arm_obj.data.bones}
zero = sorted(all_bones - used)
print(f"{len(zero)} unused bones (sample): {zero[:20]}")
