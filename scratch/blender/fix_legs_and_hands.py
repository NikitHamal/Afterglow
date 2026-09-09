"""
Fix Goat-chan legs (add toe bones, re-paint foot/toe weights) and hands (re-weight
fingers that have 0 weights + symmetrize L<->R).
Operates on the .blend, then re-exports .glb.
"""
import bpy, os, sys
from mathutils import Vector

SRC_BLEND = r"F:/Afterglow/assets/goatchan/goatchan.blend"
OUT_GLB   = r"F:/Afterglow/assets/goatchan/goatchan.glb"

bpy.ops.wm.open_mainfile(filepath=SRC_BLEND)
bpy.ops.object.mode_set(mode='OBJECT')

# --- locate body mesh and armature
meshes = [o for o in bpy.data.objects if o.type == 'MESH']
meshes.sort(key=lambda o: -len(o.data.vertices))
body = meshes[0]
arm  = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
print(f"[fix] body={body.name} verts={len(body.data.vertices)} arm={arm.name}")

# Helpers: ensure vertex group exists
def ensure_vg(name):
    vg = body.vertex_groups.get(name)
    if vg is None:
        vg = body.vertex_groups.new(name=name)
    return vg

# Build bone index lookup
bone_idx = {b.name: i for i, b in enumerate(arm.data.bones)}

# Helper: read weight of vert vg for given vert index
def get_w(vg, vi):
    for g in body.data.vertices[vi].groups:
        if g.group == vg.index:
            return g.weight
    return 0.0

def set_w(vg, vi, w):
    if w > 0:
        vg.add([vi], w, 'REPLACE')

# Make sure body is parented to armature with armature modifier
if body.parent is None or body.parent.type != 'ARMATURE':
    body.parent = arm
    body.parent_type = 'OBJECT'
# Add/refresh Armature modifier
mod = next((m for m in body.modifiers if m.type == 'ARMATURE'), None)
if mod is None:
    mod = body.modifiers.new(name='Armature', type='ARMATURE')
mod.object = arm

# 1) ADD TOE BONES 足先.L / 足先.R
bpy.ops.object.mode_set(mode='EDIT')
arm.data.edit_bones.active = None
toe_local = 0.04  # 4 cm toe length
for side, ankle_name, toe_name in [(".L","足首.L","足先.L"), (".R","足首.R","足先.R")]:
    ankle = arm.data.edit_bones.get(ankle_name)
    if ankle is None:
        print(f"[fix] WARN no ankle {ankle_name}")
        continue
    toe = arm.data.edit_bones.get(toe_name)
    if toe is None:
        toe = arm.data.edit_bones.new(toe_name)
    # Place toe forward of ankle, along -Z (toward toes)
    foot_dir = (ankle.tail - ankle.head)
    foot_dir_norm = foot_dir.normalized() if foot_dir.length > 0 else Vector((0,0,-1))
    toe.parent = ankle
    toe.head = ankle.tail.copy()
    toe.tail = ankle.tail + foot_dir_norm * toe_local
    toe.use_connect = False
    # Roll: copy from ankle so foot orientation preserved
    toe.roll = ankle.roll
    print(f"[fix] added {toe_name} head={tuple(round(x,3) for x in toe.head)} tail={tuple(round(x,3) for x in toe.tail)}")

bpy.ops.object.mode_set(mode='OBJECT')

# Ensure vertex groups exist
vg_ankleL = ensure_vg("足首.L")
vg_ankleR = ensure_vg("足首.R")
vg_toeL   = ensure_vg("足先.L")
vg_toeR   = ensure_vg("足先.R")
vg_legL   = ensure_vg("足.L")
vg_legR   = ensure_vg("足.R")
vg_kneeL  = ensure_vg("ひざ.L")
vg_kneeR  = ensure_vg("ひざ.R")
vg_thighL = ensure_vg("足.L")  # alias

# 2) RE-WEIGHT: for each foot-region vertex, split between ankle and toe.
# Foot region: x in ankle-X band, y in [-0.04, 0.13], z in [-0.04, 0.15]
# Front-half (lower z) -> 足先 with ankle remaining; back-half (higher z, near ankle joint) -> 足首
def foot_box(side):
    # MMD convention: +X=L, -X=R. So +X is Left, -X is Right.
    if side == "L":
        return (0.005, 0.085)  # x range for L foot
    else:
        return (-0.085, -0.005)

# Identify foot vertices (those dominated by 足首 or 足 or 足先)
def get_v_idx_in_group(vg):
    return [v.index for v in body.data.vertices if any(g.group == vg.index for g in v.groups)]

# 3) MIRROR: copy weights from L->R for bones that have 0 weight verts
def mirror_vg(src_vg_name, dst_vg_name, axis='x'):
    src = body.vertex_groups.get(src_vg_name)
    dst = ensure_vg(dst_vg_name)
    if src is None:
        print(f"[fix] mirror skip: no {src_vg_name}")
        return 0
    n_mirrored = 0
    for v in body.data.vertices:
        w = get_w(src, v.index)
        if w > 0:
            # vert is in source group, mirror its position to find corresponding vert on dst side
            co = v.co.copy()
            co.x = -co.x  # mirror around yz plane
            # find nearest dst-side vert with similar YZ
            best = None; best_d = 1e9
            for u in body.data.vertices:
                if abs(u.co.x) > 0.005 and (axis == 'x' and ((src_vg_name.endswith('.L') and u.co.x < 0) or (src_vg_name.endswith('.R') and u.co.x > 0))):
                    d = (u.co.yz - co.yz).length_squared
                    if d < best_d:
                        best_d = d; best = u
            if best is not None and best_d < 0.0008:  # 2.8cm tolerance
                set_w(dst, best.index, w)
                n_mirrored += 1
    print(f"[fix] mirror {src_vg_name}->{dst_vg_name}: {n_mirrored} verts")
    return n_mirrored

# 4) For foot verts: find by bbox, distribute ankle/toe
foot_l_x = (0.005, 0.090)   # +X = MMD-L
foot_r_x = (-0.090, -0.005)
foot_z = (-0.05, 0.16)      # foot is below ankle joint
foot_y = (-0.04, 0.13)

def classify_foot_verts(side):
    out = []
    if side == "L":
        xr = foot_l_x
    else:
        xr = foot_r_x
    for v in body.data.vertices:
        c = v.co
        if xr[0] <= c.x <= xr[1] and foot_y[0] <= c.y <= foot_y[1] and foot_z[0] <= c.z <= foot_z[1]:
            out.append(v)
    return out

def ankle_pos(side):
    b = arm.data.bones.get(f"足首.{side[0]}")
    if b is None: return Vector((0,0,0))
    return (b.head_local + b.tail_local) * 0.5

def toe_pos(side):
    b = arm.data.bones.get(f"足先.{side[0]}")
    if b is None: return Vector((0,0,0))
    return (b.head_local + b.tail_local) * 0.5

# Re-weight foot verts: distance-from-toe
for side, ankle_vg, toe_vg in [("L", vg_ankleL, vg_toeL), ("R", vg_ankleR, vg_toeR)]:
    fverts = classify_foot_verts(side)
    a = ankle_pos(side)
    t = toe_pos(side)
    foot_len = max((a - t).length, 1e-4)
    for v in fverts:
        d_to_toe = (v.co - t).length
        d_to_ankle = (v.co - a).length
        # weight for toe: 1 - (d_to_toe / foot_len), clamp to [0,1]
        w_toe = max(0.0, 1.0 - d_to_toe / (foot_len * 1.4))
        w_toe = min(1.0, w_toe)
        w_ankle = max(0.0, 1.0 - w_toe)
        # Also factor in existing weight of ankle — keep small leftover for leg continuity
        if w_ankle < 0.05:
            w_ankle = 0.05
        set_w(ankle_vg, v.index, w_ankle)
        set_w(toe_vg, v.index, w_toe)
    print(f"[fix] reweighted foot {side}: {len(fverts)} verts")

# 5) Mirror missing-side bone weights
for src, dst in [("足首.L","足首.R"),("足.L","足.R"),("ひざ.L","ひざ.R"),
                 ("足先.L","足先.R"),("足捩.L","足捩.R")]:
    src_v = body.vertex_groups.get(src)
    if src_v and len([v for v in body.data.vertices if any(g.group == src_v.index for g in v.groups)]) == 0:
        print(f"[fix] {src} empty, mirroring to {dst}")
        mirror_vg(src, dst)

# 6) HANDS: improve finger weights
# The hand mesh: wrist region (x ~ ±0.40, y ~ 0.07, z ~ 0.70-0.74)
# Each finger has 0/1/2 segments.
# Strategy: for each finger, identify the fingertip-cluster verts and assign
# 0.8 to tip segment, 0.2 to mid. Mid-cluster: 0.5/0.5 between 0 and 1.

# Finger bone lengths (from earlier inspect): ~0.03m
# 親指 (thumb) - separate, located at +X-low-Y for L hand
# Other fingers stack along +Y for L hand

# Let's use a coord-based per-finger heuristic
finger_anchors = {
    "L": {  # MMD-L = +X
        "親指": dict(x=0.39, y=0.05, tip_z=0.69),   # thumb sticks out to +X-lowY
        "人指": dict(x=0.42, y=0.05, tip_z=0.69),   # index
        "中指": dict(x=0.43, y=0.08, tip_z=0.69),   # middle
        "薬指": dict(x=0.44, y=0.10, tip_z=0.69),   # ring
        "小指": dict(x=0.43, y=0.12, tip_z=0.69),   # pinky (slightly +X for curl back)
    },
    "R": {  # MMD-R = -X
        "親指": dict(x=-0.39, y=0.05, tip_z=0.69),
        "人指": dict(x=-0.42, y=0.05, tip_z=0.69),
        "中指": dict(x=-0.43, y=0.08, tip_z=0.69),
        "薬指": dict(x=-0.44, y=0.10, tip_z=0.69),
        "小指": dict(x=-0.43, y=0.12, tip_z=0.69),
    },
}

def find_finger_verts(side, finger):
    a = finger_anchors[side][finger]
    # region around finger anchor
    out = []
    for v in body.data.vertices:
        c = v.co
        if (side == "L" and 0.36 < c.x < 0.46) or (side == "R" and -0.46 < c.x < -0.36):
            if abs(c.y - a["y"]) < 0.02 and abs(c.z - a["tip_z"]) < 0.025:
                out.append(v)
    return out

# Ensure 0/1/2 vertex groups exist
for side in ["L","R"]:
    for f in ["親指","人指","中指","薬指","小指"]:
        for seg in ["０","１","２"]:
            ensure_vg(f"{f}{seg}.{side}")

# Re-weight fingers: closest to tip -> seg2, mid -> seg1, base -> seg0
finger_z_base = 0.715  # base of finger
for side in ["L","R"]:
    for f in ["親指","人指","中指","薬指","小指"]:
        fverts = find_finger_verts(side, f)
        if not fverts:
            continue
        a = finger_anchors[side][f]
        for v in fverts:
            # distance from base
            z_off = finger_z_base - v.co.z  # larger = closer to tip
            z_off = max(0.0, z_off)
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

# 7) HANDS: thicken slightly via scale of hand-cluster verts
# This makes thin hands look meatier without changing topology
bpy.ops.object.mode_set(mode='OBJECT')
body.select_set(True)
bpy.context.view_layer.objects.active = body
# Scale hand verts slightly larger in X and Y to thicken them
# Use proportional edit? Simpler: just scale a subset
# For each hand-side vert, scale around its centroid
for side in ["L","R"]:
    hand_verts = [v for v in body.data.vertices
                  if (side == "L" and 0.35 < v.co.x < 0.50) or (side == "R" and -0.50 < v.co.x < -0.35)
                  and 0.68 < v.co.z < 0.75 and -0.02 < v.co.y < 0.16]
    if not hand_verts: continue
    cx = sum(v.co.x for v in hand_verts)/len(hand_verts)
    cy = sum(v.co.y for v in hand_verts)/len(hand_verts)
    cz = sum(v.co.z for v in hand_verts)/len(hand_verts)
    for v in hand_verts:
        # thicken by 1.18x in Y, 1.10x in X
        ny = cy + (v.co.y - cy) * 1.18
        nx = cx + (v.co.x - cx) * 1.10
        v.co = Vector((nx, ny, v.co.z))
    print(f"[fix] thickened hand {side}: {len(hand_verts)} verts")

# 8) Save the .blend and export .glb
bpy.ops.wm.save_as_mainfile(filepath=SRC_BLEND)
print(f"[fix] saved blend")

# Export glb
bpy.ops.object.select_all(action='DESELECT')
body.select_set(True)
bpy.context.view_layer.objects.active = body
arm.select_set(True)
bpy.ops.export_scene.gltf(
    filepath=OUT_GLB,
    use_selection=True,
    export_format='GLB',
    export_animations=False,
    export_skins=True,
    export_morph=False,
    export_morph_normal=False,
    export_apply=False,
    export_yup=True,
    export_normals=True,
    export_tangents=False,
    export_texcoords=True,
    export_colors=False,
    export_materials='EXPORT',
    export_cameras=False,
    export_lights=False,
    export_draco_mesh_compression_enable=False,
)
print(f"[fix] exported glb -> {OUT_GLB}")
print(f"[fix] size: {os.path.getsize(OUT_GLB)} bytes")
