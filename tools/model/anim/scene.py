# Scene assembly + mesh-based helpers (evaluation, settling, landmarks, verification) for clip authoring.
import bpy, json, math, os
import numpy as np
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree

NAT = "/root/agb/native"
BED_C = Vector((0.0, 1.22, 0.0)); MAT_W, MAT_L, MAT_TOP = 1.80, 2.00, 0.60
BED_TOP = MAT_TOP + 0.004
HEAD_Y, FOOT_Y = BED_C.y + MAT_L / 2, BED_C.y - MAT_L / 2

def append_blend(path, kinds=("OBJECT",)):
    with bpy.data.libraries.load(path, link=False) as (src, dst):
        dst.objects = list(src.objects)
    out = []
    for ob in dst.objects:
        if ob is None: continue
        if ob.type == "LIGHT": continue
        bpy.context.scene.collection.objects.link(ob); out.append(ob)
    return out

def load_scene(room=True):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    V = append_blend(f"{NAT}/vivian_native.blend"); M = append_blend(f"{NAT}/marcus_native.blend")
    rb = f"{NAT}/room/room.blend"
    if room and os.path.exists(rb):
        append_blend(rb)
    else:
        bpy.ops.mesh.primitive_cube_add(size=1, location=(BED_C.x, BED_C.y, MAT_TOP - 0.12))
        b = bpy.context.object; b.scale = (MAT_W, MAT_L, 0.24); b.name = "bed_proxy"
        m = bpy.data.materials.new("proxy"); m.use_nodes = True
        m.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.35, 0.07, 0.1, 1)
        b.data.materials.append(m)
    objs = {o.name: o for o in bpy.context.scene.objects}
    return objs

def body_meshes(prefix):
    return [o for o in bpy.context.scene.objects if o.type == "MESH" and o.name.startswith(prefix)]

def set_keys(obj_prefix, values):
    for o in body_meshes(obj_prefix):
        sk = o.data.shape_keys
        if not sk: continue
        for kb in sk.key_blocks[1:]:
            kb.value = values.get(kb.name, 0.0)

def evaluated(obj):
    deps = bpy.context.evaluated_depsgraph_get()
    e = obj.evaluated_get(deps); me = e.to_mesh()
    V = np.empty(len(me.vertices) * 3, np.float32); me.vertices.foreach_get("co", V); V = V.reshape(-1, 3)
    P = [tuple(p.vertices) for p in me.polygons]
    e.to_mesh_clear()
    M = np.array(obj.matrix_world)
    return V @ M[:3, :3].T + M[:3, 3], P

def apply_rigs(*bodies):
    for b in bodies: b.rig.apply()
    bpy.context.view_layer.update()

CORE_BONES = ("下半身", "上半身", "上半身2", "首", "頭", "腕.L", "腕.R", "足.L", "足.R", "乳親.L", "乳親.R", "腰")

def settle(body, target_z=BED_TOP, sink=0.012, region=None):
    """move the body vertically so its lowest CORE vertex (pelvis/torso/head/upper limbs; hands and feet are
    posed afterwards to world targets) rests at target_z - sink"""
    apply_rigs(body)
    V, _ = evaluated(body.obj)
    if region is None: region = getattr(body, "core", None)
    z = V[:, 2] if region is None else V[region, 2]
    dz = (target_z - sink) - float(z.min())
    body.move(Vector((0, 0, dz)))
    return dz

# ---------------------------------------------------------------- verification
def vgroup_mask(obj, names, thr=0.3):
    idx = {obj.vertex_groups[n].index for n in names if n in obj.vertex_groups}
    m = np.zeros(len(obj.data.vertices), bool)
    for v in obj.data.vertices:
        for g in v.groups:
            if g.group in idx and g.weight > thr: m[v.index] = True; break
    return m

def attr_mask(obj, names, thr=0.05):
    m = np.zeros(len(obj.data.vertices), bool)
    for n in names:
        a = obj.data.attributes.get(n)
        if a is None: continue
        v = np.empty(len(a.data), np.float32); a.data.foreach_get("value", v); m |= v > thr
    return m

def dominant_bone(obj):
    """per-vertex name of the strongest deform group"""
    names = {g.index: g.name for g in obj.vertex_groups}
    out = []
    for v in obj.data.vertices:
        best = max(v.groups, key=lambda g: g.weight, default=None)
        out.append(names.get(best.group, "?") if best else "?")
    return out

def interpenetration(A, Pa, allowA, B, Pb, allowB, sample=2, boneA=None, boneB=None, handA=None, handB=None, extra=()):
    """intersecting triangle pairs not covered by the allowed regions (hands may touch anything: holding is
    intended contact), max depth of A-verts inside B, and the worst offending bone pairs"""
    ta = BVHTree.FromPolygons([tuple(v) for v in A], Pa); tb = BVHTree.FromPolygons([tuple(v) for v in B], Pb)
    pairs = ta.overlap(tb)
    bad = []
    for i, j in pairs:
        if handA is not None and any(handA[k] for k in Pa[i]): continue
        if handB is not None and any(handB[k] for k in Pb[j]): continue
        okA = any(allowA[k] for k in Pa[i]); okB = any(allowB[k] for k in Pb[j])
        if okA and okB: continue
        if any(any(za[k] for k in Pa[i]) and any(zb[k] for k in Pb[j]) for za, zb in extra): continue
        bad.append((i, j))
    depth = 0.0
    if bad:
        verts = {k for i, _ in bad for k in Pa[i]}
        for k in list(verts)[::sample]:
            loc, nrm, _, d = tb.find_nearest(tuple(A[k]))
            if loc is None: continue
            s = (Vector(A[k]) - loc).dot(nrm)
            if s < 0: depth = max(depth, -s)
    top = {}
    if boneA is not None and boneB is not None:
        for i, j in bad:
            k = (boneA[Pa[i][0]], boneB[Pb[j][0]]); top[k] = top.get(k, 0) + 1
    worst = sorted(top.items(), key=lambda kv: -kv[1])[:4]
    return len(pairs), len(bad), depth, worst

def flips(prev, cur, limit_deg=35.0):
    """largest per-bone rotation change between two basis snapshots (degrees)"""
    worst = 0.0; who = None
    for n, m in cur.items():
        if n not in prev: continue
        q0 = prev[n].to_quaternion(); q1 = m.to_quaternion()
        a = math.degrees(q0.rotation_difference(q1).angle); a = min(a, 360.0 - a)
        if a > worst: worst, who = a, n
    return worst, who
