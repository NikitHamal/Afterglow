"""Re-export Goat-chan GLB from the PRISTINE .blend with settings that preserve
normals + shape keys. Non-destructive: never writes back to the .blend.

Usage:
  blender -b --factory-startup --python export_glb.py -- <src.blend> <out.glb> [--morph 1]
"""
import bpy, sys, os, json

argv = sys.argv
argv = argv[argv.index("--") + 1:] if "--" in argv else []
SRC = argv[0]
OUT = argv[1]
WANT_MORPH = not (len(argv) > 2 and argv[2] == "0")

bpy.ops.wm.open_mainfile(filepath=SRC)
bpy.ops.object.mode_set(mode='OBJECT')

report = {"src": SRC, "out": OUT, "morph": WANT_MORPH, "objects": []}

meshes = [o for o in bpy.data.objects if o.type == 'MESH']
meshes.sort(key=lambda o: -len(o.data.vertices))
body = meshes[0]
arm = next((o for o in bpy.data.objects if o.type == 'ARMATURE'), None)

# --- inspect normals state on the body
m = body.data
has_custom = m.has_custom_normals if hasattr(m, 'has_custom_normals') else None
report["body"] = body.name
report["verts"] = len(m.vertices)
report["polys"] = len(m.polygons)
report["has_custom_normals"] = bool(has_custom) if has_custom is not None else None
report["shape_keys"] = (body.data.shape_keys.name if body.data.shape_keys else None)
report["n_shape_keys"] = (len(body.data.shape_keys.key_blocks) if body.data.shape_keys else 0)
report["key_names"] = ([k.name for k in body.data.shape_keys.key_blocks] if body.data.shape_keys else [])
report["modifiers"] = [(md.name, md.type) for md in body.modifiers]
report["materials"] = [s.material.name if s.material else None for s in body.material_slots]
report["smooth_polys"] = sum(1 for p in m.polygons if p.use_smooth)
report["autosmooth"] = getattr(m, 'use_auto_smooth', 'n/a')

# Which objects are skinned to the armature
for o in meshes:
    mods = [md.type for md in o.modifiers]
    report["objects"].append({"name": o.name, "verts": len(o.data.vertices),
                              "mods": mods, "parent": o.parent.name if o.parent else None})

# --- export: ONLY skinned meshes (objects carrying an Armature modifier).
# The .blend also holds ~60 helper objects (CL*/RB*/rigidbody_bone*/smoke
# domains/collision spheres/studio floor). The shipped GLB has always been
# the 8 skinned meshes only; exporting everything adds 3 junk meshes and
# 3 unused materials. Filtering here reproduces the shipped mesh set.
skinned = [o for o in meshes if any(md.type == 'ARMATURE' for md in o.modifiers)]
report["skinned"] = [o.name for o in skinned]
report["skipped"] = [o.name for o in meshes if o not in skinned]
if not skinned:
    raise SystemExit("no skinned meshes found")

bpy.ops.object.select_all(action='DESELECT')
for o in skinned:
    o.select_set(True)
if arm:
    arm.select_set(True)
bpy.context.view_layer.objects.active = skinned[0]
body = skinned[0]

kwargs = dict(
    filepath=OUT,
    use_selection=True,
    export_format='GLB',
    export_animations=False,
    export_skins=True,
    export_morph=WANT_MORPH,
    export_morph_normal=WANT_MORPH,
    export_morph_tangent=False,
    export_apply=False,
    export_yup=True,
    export_normals=True,
    export_tangents=False,
    export_texcoords=True,
    export_colors=True,
    export_materials='EXPORT',
    export_cameras=False,
    export_lights=False,
    export_draco_mesh_compression_enable=False,
)
# Blender 4.x+ renamed a few flags; drop unsupported ones defensively.
while True:
    try:
        bpy.ops.export_scene.gltf(**kwargs)
        break
    except TypeError as ex:
        msg = str(ex)
        drop = None
        for k in list(kwargs):
            if k in msg:
                drop = k
                break
        if drop is None:
            raise
        kwargs.pop(drop)
        report.setdefault("dropped_flags", []).append(drop)

report["out_size"] = os.path.getsize(OUT)
print("EXPORT>>>")
print(json.dumps(report, indent=1, ensure_ascii=False))
print("<<<EXPORT")
