"""Audit Goat-chan GLB: measure hand + lower-leg geometry and skin weight coverage
in REST pose. Compares two GLBs so we can tell 'mesh is thin' from 'rig crushes it'.

Usage: blender -b --python audit_glb.py -- <glbA> <labelA> <glbB> <labelB>
Outputs one JSON blob between AUDIT>>> and <<<AUDIT.
"""
import bpy, sys, json, math
from mathutils import Vector

argv = sys.argv
argv = argv[argv.index("--") + 1:] if "--" in argv else []

FINGER_ROOTS = ["親指", "人指", "中指", "薬指", "小指"]
FINGER_SEGS = ["０", "１", "２"]


def wipe():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def import_glb(path):
    bpy.ops.import_scene.gltf(filepath=path)
    arm = next((o for o in bpy.data.objects if o.type == 'ARMATURE'), None)
    meshes = [o for o in bpy.data.objects if o.type == 'MESH']
    meshes.sort(key=lambda o: -len(o.data.vertices))
    return arm, meshes


def bone_world(arm, name):
    b = arm.data.bones.get(name)
    if b is None:
        return None, None
    mw = arm.matrix_world
    return mw @ b.head_local, mw @ b.tail_local


def vg_verts(obj, names, thr=0.01):
    """Vert indices with weight > thr in ANY of the named vertex groups."""
    idxs = {obj.vertex_groups[n].index for n in names if n in obj.vertex_groups}
    if not idxs:
        return []
    out = []
    for v in obj.data.vertices:
        for g in v.groups:
            if g.group in idxs and g.weight > thr:
                out.append(v.index)
                break
    return out


def bbox(obj, idxs):
    if not idxs:
        return None
    xs = [obj.data.vertices[i].co.x for i in idxs]
    ys = [obj.data.vertices[i].co.y for i in idxs]
    zs = [obj.data.vertices[i].co.z for i in idxs]
    return {
        "n": len(idxs),
        "x": [round(min(xs), 4), round(max(xs), 4)],
        "y": [round(min(ys), 4), round(max(ys), 4)],
        "z": [round(min(zs), 4), round(max(zs), 4)],
        "dim": [round(max(xs) - min(xs), 4), round(max(ys) - min(ys), 4), round(max(zs) - min(zs), 4)],
    }


def seg_profile(obj, idxs, a, b):
    """Cross-section radius stats of verts projected onto segment a->b."""
    if not idxs or a is None or b is None:
        return None
    ax = (b - a)
    L = ax.length
    if L < 1e-6:
        return None
    ax = ax / L
    radii = []
    for i in idxs:
        p = obj.matrix_world @ obj.data.vertices[i].co - a
        t = p.dot(ax)
        perp = (p - ax * t).length
        radii.append((t, perp))
    if not radii:
        return None
    radii.sort(key=lambda r: r[0])
    # split into 4 bands along the bone; report mean perpendicular radius per band
    bands = []
    for k in range(4):
        lo = L * k / 4.0
        hi = L * (k + 1) / 4.0
        sel = [r for t, r in radii if lo <= t <= hi]
        bands.append(round(sum(sel) / len(sel), 4) if sel else None)
    return {"len": round(L, 4), "bandRadius": bands}


def audit(path, label):
    wipe()
    arm, meshes = import_glb(path)
    res = {"label": label, "path": path, "bones": 0, "meshes": []}
    if arm is None:
        res["error"] = "no armature"
        return res
    res["bones"] = len(arm.data.bones)
    res["boneNames"] = sorted(b.name for b in arm.data.bones)

    body = meshes[0]
    res["bodyMesh"] = body.name
    res["bodyVerts"] = len(body.data.vertices)
    res["bodyPolys"] = len(body.data.polygons)

    # --- per-side hand + leg measurements
    for side in ["L", "R"]:
        hand_groups = [f"{f}{s}.{side}" for f in FINGER_ROOTS for s in FINGER_SEGS] + [f"手首.{side}"]
        hi = vg_verts(body, hand_groups)
        hb = bbox(body, hi)
        res[f"hand_{side}"] = hb
        # coverage: how many verts per finger segment group
        cov = {}
        for f in FINGER_ROOTS:
            for s in FINGER_SEGS:
                gname = f"{f}{s}.{side}"
                cov[gname] = len(vg_verts(body, [gname]))
        cov[f"手首.{side}"] = len(vg_verts(body, [f"手首.{side}"]))
        res[f"fingerCov_{side}"] = cov

        # lower leg: knee + shin + ankle groups
        leg_groups = [f"ひざ.{side}", f"足.{side}", f"足首.{side}", f"足先.{side}", f"つま先.{side}"]
        li = vg_verts(body, leg_groups)
        res[f"leg_{side}"] = bbox(body, li)
        knee_h, knee_t = bone_world(arm, f"ひざ.{side}")
        ank_h, ank_t = bone_world(arm, f"足首.{side}")
        toe_h, toe_t = bone_world(arm, f"足先.{side}")
        res[f"knee_{side}"] = [round(c, 4) for c in knee_h] if knee_h else None
        res[f"ankle_{side}"] = [round(c, 4) for c in ank_h] if ank_h else None
        res[f"toe_{side}"] = [round(c, 4) for c in toe_h] if toe_h else None
        if knee_h and ank_h:
            res[f"shinProfile_{side}"] = seg_profile(body, li, knee_h, ank_h)
        if ank_h and toe_h:
            res[f"footProfile_{side}"] = seg_profile(body, li, ank_h, toe_h)

    # --- global weight sanity: verts with no weights at all
    zero = sum(1 for v in body.data.vertices if not v.groups)
    res["vertsWithNoWeights"] = zero
    res["maxGroupsPerVert"] = max((len(v.groups) for v in body.data.vertices), default=0)

    return res


out = []
if len(argv) >= 2:
    for i in range(0, len(argv), 2):
        out.append(audit(argv[i], argv[i + 1] if i + 1 < len(argv) else argv[i]))
print("AUDIT>>>")
print(json.dumps(out, indent=1))
print("<<<AUDIT")
