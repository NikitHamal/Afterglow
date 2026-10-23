# Story outfits built from the body itself: each garment is the matching patch of skin, lifted off it a few mm,
# with the body's own skin weights and every body morph copied, so it follows every pose, clip and body shape
# exactly (skin-tight cut). Exports characters/outfits/<who>_<style>.glb (armature + garment meshes, no clips).
#   blender -b /root/agb/native/<who>_native.blend --python outfits.py -- <who> <out_dir>
import bpy, sys, os, json, math
import numpy as np
from mathutils import Vector
argv = sys.argv[sys.argv.index("--") + 1:]
WHO, OUTD = argv[0], argv[1]; os.makedirs(OUTD, exist_ok=True)
def log(*a): print("[outfit]", *a, flush=True)
body = bpy.data.objects[f"{WHO}_body"]; arm = body.parent
me = body.data
B = np.array([tuple(v.co) for v in me.vertices], np.float32)
N = np.array([tuple(v.normal) for v in me.vertices], np.float32)
names = {g.index: g.name for g in body.vertex_groups}
dom = []
for v in me.vertices:
    best = max(v.groups, key=lambda g: g.weight, default=None)
    dom.append(names[best.group] if best else "")
dom = np.array(dom)
def bone_z(n): return float((arm.matrix_world @ arm.data.bones[n].head_local).z)
def bone_p(n): return np.array(arm.matrix_world @ arm.data.bones[n].head_local)
LIMB = ("腕", "ひじ", "手首", "親指", "人指", "中指", "薬指", "小指")
is_arm = np.array([d.startswith(LIMB) for d in dom])
is_upperarm = np.array([d.startswith("腕") for d in dom])
gen = np.array([d.startswith(("竿", "玉袋")) for d in dom])
def attr(name):
    a = me.attributes.get(name)
    if a is None: return np.zeros(len(B), np.float32)
    v = np.empty(len(a.data), np.float32); a.data.foreach_get("value", v); return v
inner = attr("v_cav") > 0.02                              # canal walls stay out of every garment (the fabric bridges them)
pel = bone_z("下半身"); hipj = bone_z("足.L"); knee = bone_z("ひざ.L"); ankle = bone_z("足首.L")
neck = bone_z("首"); sh = bone_z("腕.L"); c2 = bone_z("上半身2")
crotch = hipj - 0.06
front = B[:, 1] < 0                                       # rest pose faces -Y
def gw(name):
    w = np.zeros(len(B), np.float32)
    g = body.vertex_groups.get(name)
    if g is None: return w
    for v in me.vertices:
        for e in v.groups:
            if e.group == g.index: w[v.index] = e.weight
    return w
BREAST_W = gw("乳親.L") + gw("乳親.R")
def nip():
    vs = np.where(BREAST_W > 0.25)[0]
    return B[vs] if len(vs) else None

GARMENTS = {}
BOTTOMS = {"knit_sweater_jeans": ((0.10, 0.16, 0.30), 0.8, "denim"), "flannel_jeans": ((0.09, 0.14, 0.26), 0.8, "denim"),
           "suit_open_collar": ((0.05, 0.055, 0.07), 0.6, "wool"), "linen_shirt_shorts": ((0.62, 0.56, 0.44), 0.85, "linen")}
if WHO == "vivian":
    bz = nip(); nz = float(np.percentile(bz[:, 2], 60)) if bz is not None else c2
    under_bust = nz - 0.075
    cup_ = np.zeros(len(B), bool)
    if bz is not None:
        for sx in (1, -1):
            c = bz[np.sign(bz[:, 0]) == sx]
            if len(c): cc = c.mean(0); cup_ |= (np.linalg.norm(B - cc, axis=1) < 0.085) & (np.sign(B[:, 0]) == sx) & front
    band = (B[:, 2] > under_bust - 0.03) & (B[:, 2] < under_bust + 0.005) & ~is_arm
    straps = (np.abs(np.abs(B[:, 0]) - 0.085) < 0.011) & (B[:, 2] > nz) & (B[:, 2] < sh + 0.06) & ~is_arm
    bra = cup_ | band | straps
    briefs = (B[:, 2] < pel + 0.015) & (B[:, 2] > crotch - 0.05) & ~is_arm & \
             ~((np.abs(B[:, 0]) > 0.075) & (B[:, 2] < hipj - 0.02))     # leg openings cut high on the hip
    dress = (B[:, 2] > knee + 0.17) & (B[:, 2] < nz + 0.055) & ~is_arm | (cup_ & ~is_arm)
    dress &= ~((B[:, 2] > nz + 0.01) & ~front & (B[:, 2] > nz + 0.03))  # low back
    GARMENTS = {
        "evening_dress": {"outer": (dress, 0.0045, (0.28, 0.03, 0.06), 0.35, "satin"), "under": (bra | briefs, 0.0018, (0.02, 0.018, 0.022), 0.45, "lace")},
        "sundress_bikini": {"outer": ((B[:, 2] > knee + 0.10) & (B[:, 2] < nz + 0.06) & ~is_arm | (cup_ & ~is_arm) | straps, 0.005, (0.92, 0.88, 0.70), 0.7, "cotton"),
                            "under": (bra | briefs, 0.0018, (0.05, 0.30, 0.55), 0.4, "lycra")},
        "knit_sweater_jeans": {"outer": (((B[:, 2] > pel - 0.06) & (B[:, 2] < neck - 0.02) & ~(is_arm & ~is_upperarm & (B[:, 2] < sh - 0.30))) |
                                         ((B[:, 2] > ankle + 0.03) & (B[:, 2] < pel + 0.02) & ~is_arm), 0.006, (0.55, 0.42, 0.30), 0.85, "knit"),
                               "under": (bra | briefs, 0.0018, (0.75, 0.62, 0.58), 0.5, "cotton")},
    }
else:
    shirt = (B[:, 2] > pel - 0.04) & (B[:, 2] < neck - 0.03) & ~(is_arm & ~is_upperarm)
    trousers = (B[:, 2] > ankle + 0.04) & (B[:, 2] < pel + 0.03) & ~is_arm & ~gen
    shorts = (B[:, 2] > knee + 0.08) & (B[:, 2] < pel + 0.03) & ~is_arm & ~gen
    boxers = (B[:, 2] > hipj - 0.16) & (B[:, 2] < pel + 0.02) & ~is_arm & ~gen
    GARMENTS = {
        "flannel_jeans": {"outer": (shirt | trousers, 0.007, (0.30, 0.06, 0.05), 0.85, "flannel"), "under": (boxers, 0.003, (0.08, 0.08, 0.09), 0.6, "cotton")},
        "suit_open_collar": {"outer": (shirt | trousers, 0.007, (0.80, 0.82, 0.86), 0.55, "cotton"), "under": (boxers, 0.003, (0.85, 0.85, 0.86), 0.6, "cotton")},
        "linen_shirt_shorts": {"outer": (shirt | shorts, 0.007, (0.86, 0.84, 0.78), 0.8, "linen"), "under": (boxers, 0.003, (0.12, 0.20, 0.32), 0.6, "cotton")},
    }

def fabric_mat(name, color, rough, kind):
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; bs = nt.nodes["Principled BSDF"]
    bs.inputs["Base Color"].default_value = (*color, 1); bs.inputs["Roughness"].default_value = rough
    bs.inputs["Metallic"].default_value = 0.0
    if kind in ("satin", "lycra"): bs.inputs["Specular IOR Level"].default_value = 0.6
    return m

def make(style, layer, mask, off, color, rough, kind):
    mask = mask & ~inner
    faces = [p for p in me.polygons if all(mask[i] for i in p.vertices)]
    vids = sorted({i for p in faces for i in p.vertices}); remap = {v: k for k, v in enumerate(vids)}
    P = B[vids] + N[vids] * off
    gm = bpy.data.meshes.new(f"{WHO}_outfit_{style}_{layer}")
    gm.from_pydata([tuple(x) for x in P], [], [tuple(remap[i] for i in p.vertices) for p in faces]); gm.update()
    for p in gm.polygons: p.use_smooth = True
    ob = bpy.data.objects.new(gm.name, gm); bpy.context.scene.collection.objects.link(ob)
    gs = {}
    for k, vi in enumerate(vids):                              # the body's own skin weights
        for g in me.vertices[vi].groups:
            n = names[g.group]
            vg = gs.get(n) or ob.vertex_groups.new(name=n); gs[n] = vg
            vg.add([k], g.weight, "REPLACE")
    sk = me.shape_keys
    if sk:                                                     # every body morph rides along (same names, same weights)
        ob.shape_key_add(name="Basis", from_mix=False)
        b0 = np.array([tuple(sk.key_blocks[0].data[i].co) for i in vids], np.float32)
        for kb in sk.key_blocks[1:]:
            d = np.array([tuple(kb.data[i].co) for i in vids], np.float32) - b0
            if np.abs(d).max() < 2e-4: continue
            nk = ob.shape_key_add(name=kb.name, from_mix=False); nk.data.foreach_set("co", (P + d).ravel())
    ob.parent = arm; ob.matrix_parent_inverse.identity()
    am = ob.modifiers.new("Armature", "ARMATURE"); am.object = arm
    ob.data.materials.append(fabric_mat(f"{WHO}_outfit_{style}_{layer}", color, rough, kind))
    lower = BOTTOMS.get(style) if layer == "outer" else None
    if lower:                                                 # separate trousers / jeans colour below the waist
        ob.data.materials.append(fabric_mat(f"{WHO}_outfit_{style}_{layer}_bottom", *lower))
        for pg in ob.data.polygons:
            if np.mean([P[i][2] for i in pg.vertices]) < pel - 0.01: pg.material_index = 1
    ob["afterglow"] = json.dumps({"outfit": {"style": style, "layer": layer, "hidesWhen": ["partial", "nude"] if layer == "outer" else ["nude"]}})
    log(f"{style}/{layer}: {len(faces)} faces, {len(vids)} verts, {len(ob.data.shape_keys.key_blocks) - 1 if ob.data.shape_keys else 0} morphs")
    return ob

for style, layers in GARMENTS.items():
    made = [make(style, layer, *spec) for layer, spec in layers.items()]
    bpy.ops.object.select_all(action="DESELECT")
    arm.select_set(True)
    for o in made: o.select_set(True)
    bpy.context.view_layer.objects.active = arm
    out = f"{OUTD}/{WHO}_{style}.glb"
    bpy.ops.export_scene.gltf(filepath=out, export_format="GLB", use_selection=True, export_yup=True, export_apply=False,
                              export_skins=True, export_morph=True, export_morph_normal=False, export_try_sparse_sk=True,
                              export_animations=False, export_extras=True, export_texcoords=False, export_normals=True,
                              export_materials="EXPORT", export_cameras=False, export_lights=False)
    log(f"exported {out} ({os.path.getsize(out)/1e6:.2f} MB)")
    sc = bpy.context.scene                                    # quick look: front three-quarter, rest pose
    sc.render.engine = "CYCLES"; sc.cycles.samples = 16; sc.cycles.use_denoising = True; sc.cycles.device = "CPU"
    sc.render.resolution_x, sc.render.resolution_y = 520, 900
    if not bpy.data.objects.get("ocam"):
        cam = bpy.data.cameras.new("ocam"); cam.lens = 50; co = bpy.data.objects.new("ocam", cam); sc.collection.objects.link(co)
        ld = bpy.data.lights.new("okey", "AREA"); ld.energy = 900; ld.size = 2.0; lo = bpy.data.objects.new("okey", ld); sc.collection.objects.link(lo)
        lo.location = (1.5, -2.5, 2.2); lo.rotation_euler = (Vector((0, 0, 1.0)) - lo.location).to_track_quat("-Z", "Y").to_euler()
        if sc.world is None: sc.world = bpy.data.worlds.new("w")
        sc.world.use_nodes = True; sc.world.node_tree.nodes["Background"].inputs[0].default_value = (0.35, 0.35, 0.38, 1)
    co = bpy.data.objects["ocam"]; sc.camera = co
    co.location = Vector((1.1, -3.0, 1.05)); co.rotation_euler = (Vector((0, 0, 0.92)) - co.location).to_track_quat("-Z", "Y").to_euler()
    sc.render.filepath = f"{OUTD}/{WHO}_{style}.png"; bpy.ops.render.render(write_still=True)
    for o in made: bpy.data.objects.remove(o)
