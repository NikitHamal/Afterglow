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
focus = ["手首.L","手首.R","親指０.L","親指１.L","中指１.L","人指１.L","小指１.L","薬指１.L",
         "足首.L","足首.R","足.L","足.R","ひざ.L","ひざ.R","下半身","上半身","上半身2"]
for n in focus:
    vs = buckets.get(n, [])
    if not vs: print(f"  {n}: NO verts"); continue
    xs=[v.x for v in vs]; ys=[v.y for v in vs]; zs=[v.z for v in vs]
    print(f"  {n}: n={len(vs):4d}  x[{min(xs):+.3f},{max(xs):+.3f}] y[{min(ys):+.3f},{max(ys):+.3f}] z[{min(zs):+.3f},{max(zs):+.3f}]")

# What % of hand-area vertices go to fingers vs wrist?
print("\n--- Finger area verts ---")
for prefix in ["親指","人指","中指","薬指","小指"]:
    c = sum(len(buckets.get(f"{prefix}０.L",[])) + len(buckets.get(f"{prefix}１.L",[])) + len(buckets.get(f"{prefix}２.L",[])) +
            len(buckets.get(f"{prefix}０.R",[])) + len(buckets.get(f"{prefix}１.R",[])) + len(buckets.get(f"{prefix}２.R",[])) for _ in [0])
print(f"Finger total verts: {c}")
print(f"Wrist L+R: {len(buckets.get('手首.L',[])) + len(buckets.get('手首.R',[]))}")

# Are there foot-toe bones that are NOT being used?
print("\n--- Bones with 0 vertices weighted ---")
used = set(buckets.keys())
all_bones = {b.name for b in arm_obj.data.bones}
zero = sorted(all_bones - used)
print(f"{len(zero)} unused bones (sample): {zero[:20]}")
