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
focus = ["手首.L","手首.R",
         "親指０.L","親指１.L","親指２.L","人指０.L","人指１.L","人指２.L",
         "中指０.L","中指１.L","中指２.L","薬指０.L","薬指１.L","薬指２.L",
         "小指０.L","小指１.L","小指２.L",
         "親指０.R","親指１.R","人指１.R","中指１.R","薬指１.R","小指１.R",
         "足首.L","足首.R","足.L","足.R","ひざ.L","ひざ.R","下半身",
         "上半身","上半身2","足捩.L","足捩.R"]
print("\n=== DOMINANT BONE BBOX ===")
for n in focus:
    vs = buckets.get(n, [])
    if not vs: print(f"  {n}: NO verts"); continue
    xs=[v.x for v in vs]; ys=[v.y for v in vs]; zs=[v.z for v in vs]
    print(f"  {n}: n={len(vs):4d}  x[{min(xs):+.3f},{max(xs):+.3f}] y[{min(ys):+.3f},{max(ys):+.3f}] z[{min(zs):+.3f},{max(zs):+.3f}]")

# Check if there's a foot toe bone
print("\n=== ALL BONE NAMES containing 足 ===")
for b in arm_obj.data.bones:
    if "足" in b.name:
        print(f"  {b.name}  parent={b.parent.name if b.parent else 'None'}  len={b.length:.3f}")

# Vertex count weighted to each finger
print("\n=== FINGER VERT COUNTS ===")
for finger in ["親指","人指","中指","薬指","小指"]:
    for side in [".L",".R"]:
        for seg in ["０","１","２"]:
            n = f"{finger}{seg}{side}"
            print(f"  {n}: {len(buckets.get(n,[]))}")

# Vertex count for hand area (any finger bone)
hand_L = sum(len(buckets.get(f"{p}{s}{side}",[])) for p in ["親指","人指","中指","薬指","小指"] for s in ["０","１","２"] for side in [".L"])
hand_R = sum(len(buckets.get(f"{p}{s}{side}",[])) for p in ["親指","人指","中指","薬指","小指"] for s in ["０","１","２"] for side in [".R"])
print(f"\nTotal finger verts: L={hand_L} R={hand_R}")
print(f"Total wrist verts: L={len(buckets.get('手首.L',[]))} R={len(buckets.get('手首.R',[]))}")
print(f"Total ankle verts: L={len(buckets.get('足首.L',[]))} R={len(buckets.get('足首.R',[]))}")
print(f"Total leg verts: L={len(buckets.get('足.L',[]))} R={len(buckets.get('足.R',[]))}")
print(f"Total knee verts: L={len(buckets.get('ひざ.L',[]))} R={len(buckets.get('ひざ.R',[]))}")
