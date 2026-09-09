"""Fix Goat-chan: add toe bones, re-paint foot/toe/finger weights, thicken hands."""
import bpy, os
from mathutils import Vector

SRC_BLEND = r"F:/Afterglow/assets/goatchan/goatchan.blend"
OUT_GLB   = r"F:/Afterglow/assets/goatchan/goatchan.glb"

bpy.ops.wm.open_mainfile(filepath=SRC_BLEND)
bpy.ops.object.mode_set(mode='OBJECT')

meshes = [o for o in bpy.data.objects if o.type == 'MESH']
meshes.sort(key=lambda o: -len(o.data.vertices))
body = meshes[0]
arm  = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
print(f"[fix] body={body.name} verts={len(body.data.vertices)} arm={arm.name}")

def ensure_vg(name):
    vg = body.vertex_groups.get(name)
    if vg is None:
        vg = body.vertex_groups.new(name=name)
    return vg

def get_w(vg, vi):
    for g in body.data.vertices[vi].groups:
        if g.group == vg.index:
            return g.weight
    return 0.0

def set_w(vg, vi, w):
    if w > 0:
        vg.add([vi], w, 'REPLACE')

# 1) ADD TOE BONES — must be in EDIT mode with armature active
bpy.ops.object.select_all(action='DESELECT')
arm.select_set(True)
bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode='EDIT')
toe_local = 0.04
for side, ankle_name, toe_name in [(".L","足首.L","足先.L"), (".R","足首.R","足先.R")]:
    ankle = arm.data.edit_bones.get(ankle_name)
    print(f"[fix] ankle {ankle_name}: found={ankle is not None}")
    if ankle is None:
        continue
    toe = arm.data.edit_bones.get(toe_name)
    if toe is None:
        toe = arm.data.edit_bones.new(toe_name)
    foot_dir = (ankle.tail - ankle.head)
    foot_dir_norm = foot_dir.normalized() if foot_dir.length > 0 else Vector((0,0,-1))
    toe.parent = ankle
    toe.head = ankle.tail.copy()
    toe.tail = ankle.tail + foot_dir_norm * toe_local
    toe.use_connect = False
    toe.roll = ankle.roll
    print(f"[fix] added {toe_name} head={tuple(round(x,3) for x in toe.head)} tail={tuple(round(x,3) for x in toe.tail)}")
bpy.ops.object.mode_set(mode='OBJECT')
bpy.ops.object.select_all(action='DESELECT')
body.select_set(True)
bpy.context.view_layer.objects.active = body

# Ensure vertex groups exist
vg_ankleL = ensure_vg("足首.L")
vg_ankleR = ensure_vg("足首.R")
vg_toeL   = ensure_vg("足先.L")
vg_toeR   = ensure_vg("足先.R")
vg_legL   = ensure_vg("足.L")
vg_legR   = ensure_vg("足.R")
vg_kneeL  = ensure_vg("ひざ.L")
vg_kneeR  = ensure_vg("ひざ.R")

# 2) Re-weight foot verts
foot_l_x = (0.005, 0.090)
foot_r_x = (-0.090, -0.005)
foot_z   = (-0.05, 0.16)
foot_y   = (-0.04, 0.13)

def classify_foot_verts(side):
    xr = foot_l_x if side == "L" else foot_r_x
    return [v for v in body.data.vertices
            if xr[0] <= v.co.x <= xr[1] and foot_y[0] <= v.co.y <= foot_y[1] and foot_z[0] <= v.co.z <= foot_z[1]]

def bone_mid(name):
    b = arm.data.bones.get(name)
    if b is None: return Vector((0,0,0))
    return (b.head_local + b.tail_local) * 0.5

for side, ankle_vg, toe_vg in [("L", vg_ankleL, vg_toeL), ("R", vg_ankleR, vg_toeR)]:
    fverts = classify_foot_verts(side)
    a = bone_mid(f"足首.{side}")
    t = bone_mid(f"足先.{side}")
    foot_len = max((a - t).length, 1e-4)
    for v in fverts:
        d_to_toe = (v.co - t).length
        w_toe = max(0.0, 1.0 - d_to_toe / (foot_len * 1.4))
        w_toe = min(1.0, w_toe)
        w_ankle = max(0.05, 1.0 - w_toe)
        set_w(ankle_vg, v.index, w_ankle)
        set_w(toe_vg, v.index, w_toe)
    print(f"[fix] reweighted foot {side}: {len(fverts)} verts")

# 3) Mirror missing-side bone weights
def mirror_vg(src_name, dst_name):
    src = body.vertex_groups.get(src_name)
    if src is None: return 0
    src_verts = [(v.co.copy(), get_w(src, v.index)) for v in body.data.vertices if get_w(src, v.index) > 0]
    if not src_verts: return 0
    dst = ensure_vg(dst_name)
    mirrored = 0
    # Build YZ-bucketed index of destination side verts
    side = "L" if dst_name.endswith(".L") else "R"
    target_x_sign = 1 if side == "L" else -1  # MMD: +X=L
    dst_verts = [v for v in body.data.vertices if (target_x_sign > 0 and v.co.x > 0.005) or (target_x_sign < 0 and v.co.x < -0.005)]
    for co, w in src_verts:
        target = Vector((-co.x, co.y, co.z))
        best = None; best_d = 1e9
        for u in dst_verts:
            d = (u.co - target).length_squared
            if d < best_d:
                best_d = d; best = u
        if best is not None and best_d < 0.0006:  # ~2.5cm
            set_w(dst, best.index, w)
            mirrored += 1
    print(f"[fix] mirror {src_name}->{dst_name}: {mirrored} verts (out of {len(src_verts)})")
    return mirrored

for src, dst in [("足首.L","足首.R"),("足.L","足.R"),("ひざ.L","ひざ.R"),
                 ("足先.L","足先.R"),("足捩.L","足捩.R")]:
    src_v = body.vertex_groups.get(src)
    if src_v:
        n = sum(1 for v in body.data.vertices if get_w(src_v, v.index) > 0)
        if n == 0:
            mirror_vg(src, dst)
        else:
            print(f"[fix] {src} has {n} verts, skip mirror")

# 4) FINGERS
finger_anchors = {
    "L": {"親指": (0.39,0.045), "人指": (0.42,0.05), "中指": (0.43,0.075),
          "薬指": (0.44,0.10), "小指": (0.43,0.125)},
    "R": {"親指": (-0.39,0.045), "人指": (-0.42,0.05), "中指": (-0.43,0.075),
          "薬指": (-0.44,0.10), "小指": (-0.43,0.125)},
}

for side in ["L","R"]:
    for f in ["親指","人指","中指","薬指","小指"]:
        for seg in ["０","１","２"]:
            ensure_vg(f"{f}{seg}.{side}")

def find_finger_verts(side, finger):
    ax, ay = finger_anchors[side][finger]
    out = []
    for v in body.data.vertices:
        c = v.co
        if side == "L":
            if not (0.36 < c.x < 0.46): continue
        else:
            if not (-0.46 < c.x < -0.36): continue
        if abs(c.y - ay) < 0.025 and 0.66 < c.z < 0.74:
            out.append(v)
    return out

for side in ["L","R"]:
    for f in ["親指","人指","中指","薬指","小指"]:
        fverts = find_finger_verts(side, f)
        if not fverts: continue
        for v in fverts:
            z_off = max(0.0, 0.715 - v.co.z)
            if z_off > 0.012:
                w2, w1, w0 = 0.85, 0.15, 0.0
            elif z_off > 0.005:
                w2, w1, w0 = 0.15, 0.7, 0.15
            else:
                w2, w1, w0 = 0.0, 0.4, 0.6
            set_w(body.vertex_groups[f"{f}０.{side}"], v.index, w0)
            set_w(body.vertex_groups[f"{f}１.{side}"], v.index, w1)
            set_w(body.vertex_groups[f"{f}２.{side}"], v.index, w2)
        print(f"[fix] reweighted {f} {side}: {len(fverts)} verts")

# 5) Thicken hands
for side in ["L","R"]:
    hand_verts = [v for v in body.data.vertices
                  if (side == "L" and 0.35 < v.co.x < 0.50) or (side == "R" and -0.50 < v.co.x < -0.35)]
    if not hand_verts: continue
    cx = sum(v.co.x for v in hand_verts)/len(hand_verts)
    cy = sum(v.co.y for v in hand_verts)/len(hand_verts)
    for v in hand_verts:
        ny = cy + (v.co.y - cy) * 1.20
        nx = cx + (v.co.x - cx) * 1.10
        v.co = Vector((nx, ny, v.co.z))
    print(f"[fix] thickened hand {side}: {len(hand_verts)} verts")

# 6) Save
bpy.ops.wm.save_as_mainfile(filepath=SRC_BLEND)
print(f"[fix] saved blend")

# 7) Export GLB (Blender 5.2 compatible kwargs)
bpy.ops.object.select_all(action='DESELECT')
body.select_set(True)
arm.select_set(True)
bpy.context.view_layer.objects.active = body
# Use only valid kwargs
try:
    bpy.ops.export_scene.gltf(
        filepath=OUT_GLB,
        use_selection=True,
        export_format='GLB',
        export_animations=False,
        export_skins=True,
        export_apply=False,
        export_yup=True,
        export_normals=True,
        export_tangents=False,
        export_texcoords=True,
        export_materials='EXPORT',
        export_cameras=False,
        export_lights=False,
    )
except TypeError as e:
    # Fallback: try minimal kwargs
    print(f"[fix] retry export: {e}")
    bpy.ops.export_scene.gltf(
        filepath=OUT_GLB,
        use_selection=True,
        export_format='GLB',
    )

print(f"[fix] exported glb -> {OUT_GLB}")
print(f"[fix] size: {os.path.getsize(OUT_GLB)} bytes")
