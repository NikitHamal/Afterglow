# Story places kit (Blender 4.2, Z-up; exported Y-up): materials, meshes, walls with openings, windows, cushions,
# lights, the Cycles lightmap bake and the export. Used by build_place.py; the bedrooms come from build_room.py
# (same conventions: lightmap on UV1 as emissive, room colour = base x lightmap, lights published for the engine).
import bpy, bmesh, math, os, json, time
import numpy as np
from mathutils import Vector

S = dict(out=None, tex=None, t0=0.0, lights=[], no_lightmap=set())


def log(*a): print("[place]", *a, flush=True)


def init(out):
    S.update(out=out, tex=os.path.join(out, "tex"), t0=time.time(), lights=[], no_lightmap=set())
    os.makedirs(S["tex"], exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)


def tex(name): return os.path.join(S["tex"], name)


def image(path, colorspace="sRGB"):
    im = bpy.data.images.load(path, check_existing=True); im.colorspace_settings.name = colorspace; return im


def mat(name, albedo=None, normal=None, color=(0.5, 0.5, 0.5), rough=0.6, metal=0.0, emit=None, emit_tex=None,
        emit_strength=1.0, normal_strength=0.6, baked=True):
    """principled material; baked=False keeps it out of the lightmap (emissive things: flames, shades, skies)"""
    # the lightmap is a diffuse bake and the engine draws rooms non-metallic: a metallic surface would bake black
    if baked: metal = 0.0
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial"); bs = nt.nodes.new("ShaderNodeBsdfPrincipled"); nt.links.new(bs.outputs[0], out.inputs[0])
    bs.inputs["Roughness"].default_value = rough; bs.inputs["Metallic"].default_value = metal
    bs.inputs["Base Color"].default_value = (*color, 1)
    if albedo:
        t = nt.nodes.new("ShaderNodeTexImage"); t.image = image(albedo); nt.links.new(t.outputs[0], bs.inputs["Base Color"])
    if normal:
        tn = nt.nodes.new("ShaderNodeTexImage"); tn.image = image(normal, "Non-Color")
        nm = nt.nodes.new("ShaderNodeNormalMap"); nm.inputs["Strength"].default_value = normal_strength
        nt.links.new(tn.outputs[0], nm.inputs["Color"]); nt.links.new(nm.outputs[0], bs.inputs["Normal"])
    if emit is not None or emit_tex:
        bs.inputs["Emission Strength"].default_value = emit_strength
        if emit_tex:
            te = nt.nodes.new("ShaderNodeTexImage"); te.image = image(emit_tex); nt.links.new(te.outputs[0], bs.inputs["Emission Color"])
        else:
            bs.inputs["Emission Color"].default_value = (*emit, 1)
    if not baked: S["no_lightmap"].add(name)
    return m


# ------------------------------------------------------------------ meshes
def link(ob): bpy.context.scene.collection.objects.link(ob); return ob


def from_bm(name, bm, material, smooth=False):
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    for p in me.polygons: p.use_smooth = smooth
    ob = link(bpy.data.objects.new(name, me)); ob.data.materials.append(material); return ob


def box_uv(ob, scale=1.0):
    """world-space box mapping: tiling textures keep one texel density everywhere"""
    me = ob.data
    uv = me.uv_layers.get("UVMap") or me.uv_layers.new(name="UVMap")
    for p in me.polygons:
        n = p.normal; ax = max(range(3), key=lambda i: abs(n[i]))
        for li in p.loop_indices:
            c = ob.matrix_world @ me.vertices[me.loops[li].vertex_index].co
            u, v = (c.y, c.z) if ax == 0 else (c.x, c.z) if ax == 1 else (c.x, c.y)
            uv.data[li].uv = (u * scale, v * scale)


def box(name, size, loc, material, bevel=0.0, segs=2, smooth=False, uv_scale=1.0, rot_z=0.0):
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
    cz, sz = math.cos(rot_z), math.sin(rot_z)
    for v in bm.verts:
        x, y, z = v.co.x * size[0], v.co.y * size[1], v.co.z * size[2]
        v.co = Vector((x * cz - y * sz, x * sz + y * cz, z)) + Vector(loc)
    if bevel > 0: bmesh.ops.bevel(bm, geom=bm.verts[:] + bm.edges[:], offset=bevel, segments=segs, affect="EDGES")
    ob = from_bm(name, bm, material, smooth or bevel > 0)
    box_uv(ob, uv_scale); return ob


def grid(name, sx, sy, nx, ny, material, fn=None, loc=(0, 0, 0), smooth=True, uv_scale=1.0, uv_fit=False):
    """sx x sy grid in the XY plane (z = fn(x, y)); UVs in metres x uv_scale, or 0..1 over the grid (uv_fit: cards)"""
    bm = bmesh.new(); bmesh.ops.create_grid(bm, x_segments=nx, y_segments=ny, size=0.5)
    for v in bm.verts:
        x, y = v.co.x * sx, v.co.y * sy
        v.co = Vector((x, y, fn(x, y) if fn else 0.0)) + Vector(loc)
    ob = from_bm(name, bm, material, smooth)
    uv = ob.data.uv_layers.new(name="UVMap")
    for li, l in enumerate(ob.data.loops):
        c = ob.data.vertices[l.vertex_index].co - Vector(loc)
        uv.data[li].uv = (c.x / sx + 0.5, c.y / sy + 0.5) if uv_fit else (c.x * uv_scale, c.y * uv_scale)
    return ob


def cylinder(name, r0, r1, h, loc, material, seg=32, cap=True, axis="Z"):
    """a cylinder / cone standing on `loc` (its origin: a rotation set afterwards turns it about its base)"""
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=cap, cap_tris=False, segments=seg, radius1=r0, radius2=r1, depth=h)
    for v in bm.verts:
        x, y, z = v.co.x, v.co.y, v.co.z + h / 2
        v.co = Vector({"Z": (x, y, z), "X": (z, x, y), "Y": (x, z, y)}[axis])
    ob = from_bm(name, bm, material, True)
    ob.location = loc; bpy.context.view_layer.update()
    box_uv(ob, 1.0); return ob


def quad(name, corners, material, inward, uv_scale=1.0):
    """one flat polygon from world corners, its normal turned toward `inward` (a direction)"""
    bm = bmesh.new(); f = bm.faces.new([bm.verts.new(c) for c in corners]); bm.normal_update()
    if f.normal.dot(Vector(inward)) < 0: f.normal_flip()
    ob = from_bm(name, bm, material); box_uv(ob, uv_scale); return ob


def cushion(name, size, loc, material, tilt=0.0, e=0.35, sag=0.35, uv_scale=1.5, rot_z=0.0):
    """soft superellipsoid cushion / pillow (thinner toward its edges)"""
    sx, sy, sz = size
    bm = bmesh.new(); bmesh.ops.create_uvsphere(bm, u_segments=40, v_segments=24, radius=1.0)
    def sp(c): return math.copysign(abs(c) ** e, c)
    for v in bm.verts:
        x, y, z = sp(v.co.x) * sx / 2, sp(v.co.y) * sy / 2, sp(v.co.z) * sz / 2
        z *= 1.0 - sag * (abs(2 * x / sx) ** 3 + abs(2 * y / sy) ** 3)
        v.co = Vector((x, y, z + 0.003 * math.sin(x * 40) * math.sin(y * 35)))
    ob = from_bm(name, bm, material, True)
    ob.rotation_euler = (tilt, 0, rot_z); ob.location = loc
    uv = ob.data.uv_layers.new(name="UVMap")
    for li, l in enumerate(ob.data.loops):
        c = ob.data.vertices[l.vertex_index].co; uv.data[li].uv = (c.x * uv_scale, c.y * uv_scale)
    return ob


# ------------------------------------------------------------------ room shell
SIDES = {"+X": (Vector((-1, 0, 0)), 0), "-X": (Vector((1, 0, 0)), 0), "+Y": (Vector((0, -1, 0)), 1), "-Y": (Vector((0, 1, 0)), 1)}


def wall(name, side, rect, h, material, holes=(), uv_scale=0.9):
    """wall `side` of the room box rect=(x0, x1, y0, y1), facing in, height h, with rectangular openings
    holes=[(a, b, z0, z1)] (a..b along the wall in world coordinates)"""
    x0, x1, y0, y1 = rect
    along_x = side in ("+Y", "-Y")
    lo, hi = (x0, x1) if along_x else (y0, y1)
    fixed = {"+X": x1, "-X": x0, "+Y": y1, "-Y": y0}[side]
    rects, u = [], lo
    for a, b, z0, z1 in sorted(holes):
        if a > u: rects.append((u, a, 0.0, h))
        if z0 > 0: rects.append((a, b, 0.0, z0))
        if z1 < h: rects.append((a, b, z1, h))
        u = b
    if u < hi: rects.append((u, hi, 0.0, h))
    bm = bmesh.new()
    for a, b, z0, z1 in rects:
        pts = [(a, z0), (b, z0), (b, z1), (a, z1)]
        bm.faces.new([bm.verts.new((p, fixed, z) if along_x else (fixed, p, z)) for p, z in pts])
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    bm.normal_update()
    inward = SIDES[side][0]
    for f in bm.faces:
        if f.normal.dot(inward) < 0: f.normal_flip()
    ob = from_bm(name, bm, material); box_uv(ob, uv_scale); return ob


def floor_and_ceiling(rect, h, floor_mat, ceil_mat=None, floor_uv=0.8):
    x0, x1, y0, y1 = rect
    quad("floor", [(x0, y0, 0), (x1, y0, 0), (x1, y1, 0), (x0, y1, 0)], floor_mat, (0, 0, 1), floor_uv)
    if ceil_mat: quad("ceiling", [(x0, y0, h), (x1, y0, h), (x1, y1, h), (x0, y1, h)], ceil_mat, (0, 0, -1), 0.9)


def window(name, side, fixed, c, zc, w, h, sky_mat, frame_mat, cols=2, rows=2, sky_dist=0.9, sky_scale=(2.6, 2.0), sill_mat=None):
    """a window in wall `side` (wall plane at `fixed`), centred at c along the wall / zc high: frame, glazing bars,
    sill and an emissive view plane `sky_dist` outside (bigger than the opening so it holds under parallax)"""
    along_x = side in ("+Y", "-Y")
    out = -SIDES[side][0]                                     # outward normal
    def P(a, z, d=0.0):                                     # a along the wall, z up, d outward from the plane
        p = Vector((a, fixed, z)) if along_x else Vector((fixed, a, z))
        return p + out * d
    def frame_box(nm, a, z, la, lz, depth, d):
        size = (la, depth, lz) if along_x else (depth, la, lz)
        box(nm, size, tuple(P(a, z, d)), frame_mat, bevel=0.006)
    t = 0.07
    frame_box(f"{name}_top", c, zc + h / 2, w + 2 * t, t, 0.12, 0.0)
    frame_box(f"{name}_bot", c, zc - h / 2, w + 2 * t, t, 0.12, 0.0)
    frame_box(f"{name}_l", c - w / 2, zc, t, h, 0.12, 0.0)
    frame_box(f"{name}_r", c + w / 2, zc, t, h, 0.12, 0.0)
    for k in range(1, cols): frame_box(f"{name}_mv{k}", c - w / 2 + k * w / cols, zc, 0.035, h, 0.05, 0.02)
    for k in range(1, rows): frame_box(f"{name}_mh{k}", c, zc - h / 2 + k * h / rows, w, 0.035, 0.05, 0.02)
    if sill_mat is not None:
        frame_box(f"{name}_sill", c, zc - h / 2 - 0.045, w + 0.3, 0.04, 0.26, -0.08)
    if sky_mat is None: return None                        # a real (3D) view outside: no card
    sw, sh = w * sky_scale[0], h * sky_scale[1]
    corners = [P(c - sw / 2, zc - sh / 2, sky_dist), P(c + sw / 2, zc - sh / 2, sky_dist), P(c + sw / 2, zc + sh / 2, sky_dist), P(c - sw / 2, zc + sh / 2, sky_dist)]
    bm = bmesh.new(); f = bm.faces.new([bm.verts.new(v) for v in corners]); bm.normal_update()
    if f.normal.dot(-out) < 0: f.normal_flip()
    sky = from_bm(f"{name}_view", bm, sky_mat)
    uv = sky.data.uv_layers.new(name="UVMap")
    for li, l in enumerate(sky.data.loops):           # the picture is stretched over the plane, seen from inside
        p = sky.data.vertices[l.vertex_index].co - corners[0]
        ua = (p.dot((corners[1] - corners[0]).normalized()) / sw)
        uv.data[li].uv = (ua, p.z / sh)
    return sky


# ------------------------------------------------------------------ lights (bake power in W; engine units set at export)
def light(kind, name, loc, color, power, radius=0.05, rot=None, spot=None, size=None):
    ld = bpy.data.lights.new(name, kind); ld.color = color; ld.energy = power
    if kind in ("POINT", "SPOT"): ld.shadow_soft_size = radius
    if kind == "SPOT": ld.spot_size = spot or 1.4; ld.spot_blend = 0.6
    if kind == "AREA" and size: ld.size = size
    ob = link(bpy.data.objects.new(name, ld)); ob.location = loc
    if rot: ob.rotation_euler = rot
    bpy.context.view_layer.update()
    d = dict(name=name, type=kind.lower(), position=[loc[0], loc[2], -loc[1]], color=list(color), power_w=power, radius=radius)
    if kind in ("SPOT", "SUN", "AREA"):
        dv = ob.matrix_world.to_3x3() @ Vector((0, 0, -1)); d["direction"] = [dv.x, dv.z, -dv.y]
    if kind == "SPOT": d["outer_rad"] = ld.spot_size / 2
    if kind == "SUN": d.pop("position")
    S["lights"].append(d); return ob


def aim(ob, target):
    ob.rotation_euler = (Vector(target) - ob.location).to_track_quat("-Z", "Y").to_euler(); bpy.context.view_layer.update()


# ------------------------------------------------------------------ bake + export
def _bake(lit, img, node, passes, samples):
    """bake DIFFUSE `passes` into `img` through each material's `node` image node (UVLightmap), on one joined copy of
    the lit meshes (one Cycles sync instead of one per object; the copy's lightmap UV layer is made active)"""
    sc = bpy.context.scene
    for o in lit:
        for m in o.data.materials:
            nt = m.node_tree
            if node not in nt.nodes:
                t = nt.nodes.new("ShaderNodeTexImage"); t.name = node
                uvn = nt.nodes.new("ShaderNodeUVMap"); uvn.uv_map = "UVLightmap"; nt.links.new(uvn.outputs[0], t.inputs[0])
            nt.nodes[node].image = img
            nt.nodes.active = nt.nodes[node]
    sc.render.engine = "CYCLES"; sc.cycles.device = "CPU"; sc.cycles.samples = samples; sc.cycles.use_denoising = False
    sc.render.bake.margin = 4
    tb = time.time()
    bpy.ops.object.select_all(action="DESELECT")
    dups = []
    for o in lit:
        d = o.copy(); d.data = o.data.copy(); sc.collection.objects.link(d); dups.append(d); o.hide_render = True
    for d in dups: d.select_set(True)
    bpy.context.view_layer.objects.active = dups[0]
    bpy.ops.object.join()
    J = bpy.context.view_layer.objects.active
    J.data.uv_layers["UVLightmap"].active = True
    bpy.ops.object.select_all(action="DESELECT"); J.select_set(True); bpy.context.view_layer.objects.active = J
    bpy.ops.object.bake(type="DIFFUSE", pass_filter=passes, use_clear=True, margin=4)
    bpy.data.objects.remove(J)
    for o in lit: o.hide_render = False
    log(f"baked {node} {img.size[0]}px x {samples} spp ({'+'.join(sorted(passes)).lower()}) in {time.time()-tb:.0f}s")


def _pixels(img):
    n = img.size[0]
    px = np.empty(n * n * 4, np.float32); img.pixels.foreach_get(px); px = px.reshape(n, n, 4)[..., :3]
    cov = (px.sum(-1) > 1e-6).astype(np.float32)
    def blur(a):                                   # light denoise weighted by coverage (keeps island edges)
        k = np.array([1, 2, 1], np.float32) / 4
        o = sum(k[i] * np.roll(a, i - 1, 0) for i in range(3)); return sum(k[i] * np.roll(o, i - 1, 1) for i in range(3))
    den = blur(px * cov[..., None]) / np.maximum(blur(cov), 1e-4)[..., None]
    return np.where(cov[..., None] > 0, den, px), cov


def _srgb(a): return np.where(a <= 0.0031308, a * 12.92, 1.055 * np.clip(a, 0, None) ** (1 / 2.4) - 0.055)


def bake_and_export(pid, meta, engine, world_color=(0.006, 0.006, 0.012), bake_res=2048, samples=96, viewer=True):
    """lightmap UVs over every lit mesh (one atlas), Cycles diffuse irradiance bake, sRGB-encoded with a scale,
    wired as emissive on UV1; then the engine light powers, room.json-style meta, <pid>_raw.glb + .blend.
    viewer: also bakes the lit colour (albedo x light) into the same atlas and writes <pid>_view.glb, a look-alike of
    the in-game room for plain glTF viewers (no engine needed); the game asset does not change."""
    from PIL import Image
    sc = bpy.context.scene
    world = bpy.data.worlds.new("world"); sc.world = world; world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (*world_color, 1)
    lit = [o for o in sc.objects if o.type == "MESH" and o.data.materials and o.data.materials[0].name not in S["no_lightmap"]]
    for o in lit:
        if "UVLightmap" not in o.data.uv_layers: o.data.uv_layers.new(name="UVLightmap")
        o.data.uv_layers["UVLightmap"].active = True
    bpy.ops.object.select_all(action="DESELECT")
    for o in lit: o.select_set(True)
    bpy.context.view_layer.objects.active = lit[0]
    bpy.ops.object.mode_set(mode="EDIT"); bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=0.004, area_weight=0.0, correct_aspect=True, scale_to_bounds=False)
    bpy.ops.uv.pack_islands(rotate=True, margin=0.004)
    bpy.ops.object.mode_set(mode="OBJECT")
    for o in lit: o.data.uv_layers["UVMap"].active = True; o.data.uv_layers["UVMap"].active_render = True
    log(f"lightmap UVs packed for {len(lit)} objects")
    lm = bpy.data.images.new("room_lightmap", bake_res, bake_res, alpha=False, float_buffer=True); lm.colorspace_settings.name = "Non-Color"
    _bake(lit, lm, "LM_TEX", {"DIRECT", "INDIRECT"}, samples)
    vb = None
    if viewer:                                     # before any emission is wired or light power changed
        vb = bpy.data.images.new("view_atlas", bake_res, bake_res, alpha=False, float_buffer=True); vb.colorspace_settings.name = "Non-Color"
        _bake(lit, vb, "VIEW_TEX", {"DIRECT", "INDIRECT", "COLOR"}, max(64, samples // 2))
    den, cov = _pixels(lm)
    scale = float(np.percentile(den[cov > 0].max(-1), 99.5)) or 1.0
    lm_path = os.path.join(S["out"], "lightmap.png")
    Image.fromarray(np.flipud(np.clip(_srgb(np.clip(den / scale, 0, 1)) * 255 + 0.5, 0, 255).astype(np.uint8))).save(lm_path, optimize=True)
    lmimg = bpy.data.images.load(lm_path); lmimg.colorspace_settings.name = "sRGB"
    done = set()
    for o in lit:
        for m in o.data.materials:
            if m.name in done: continue
            done.add(m.name); nt = m.node_tree
            nt.nodes["LM_TEX"].image = lmimg
            bs = [n for n in nt.nodes if n.type == "BSDF_PRINCIPLED"][0]
            nt.links.new(nt.nodes["LM_TEX"].outputs[0], bs.inputs["Emission Color"]); bs.inputs["Emission Strength"].default_value = 1.0
            nt.nodes.active = nt.nodes["LM_TEX"]
    bpy.data.images.remove(lm)
    log(f"lightmap encoded (scale {scale:.3f})")
    meta = dict(meta, lights=S["lights"], lightmap=dict(slot="emissiveTexture", texCoord=1, encoding="sRGB", scale=scale,
                mode="full diffuse irradiance (direct+indirect) from all static lights; room colour = base x lightmap"))
    json.dump(meta, open(os.path.join(S["out"], f"{pid}.json"), "w"), indent=1)
    sc["afterglow"] = json.dumps(meta)
    for o in [o for o in sc.objects if o.type == "LIGHT"]:          # engine units (scene.cpp lightsFromRoomGlb)
        e = next((v for k, v in engine.items() if o.name.startswith(k)), "keep")
        if e is None: bpy.data.objects.remove(o)
        elif e != "keep": o.data.energy = e
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(S["out"], f"{pid}.blend"))
    gltf = dict(export_format="GLB", use_selection=False, export_yup=True, export_apply=True, export_texcoords=True, export_normals=True,
                export_tangents=False, export_materials="EXPORT", export_cameras=False, export_import_convert_lighting_mode="RAW",
                export_image_format="AUTO")
    raw = os.path.join(S["out"], f"{pid}_raw.glb")
    bpy.ops.export_scene.gltf(filepath=raw, export_lights=True, export_extras=True, **gltf)
    log(f"{pid} exported ({time.time()-S['t0']:.0f}s): {os.path.getsize(raw)/1e6:.1f} MB")
    if vb is None: return
    den, cov = _pixels(vb)                         # lit colour: exposure from the bright end, gentle shoulder
    k = float(np.percentile(den[cov > 0].max(-1), 99.0)) or 1.0
    x = den / k * 0.9
    view_path = os.path.join(S["out"], "view_atlas.jpg")
    Image.fromarray(np.flipud(np.clip(_srgb(x / (1 + 0.25 * x)) * 255 + 0.5, 0, 255).astype(np.uint8))).save(view_path, quality=88)
    vimg = bpy.data.images.load(view_path); vimg.colorspace_settings.name = "sRGB"
    done = set()
    for o in lit:
        for m in o.data.materials:
            if m.name in done: continue
            done.add(m.name); nt = m.node_tree
            bs = [n for n in nt.nodes if n.type == "BSDF_PRINCIPLED"][0]
            for inp in ("Base Color", "Normal"):
                for l in list(bs.inputs[inp].links): nt.links.remove(l)
            bs.inputs["Base Color"].default_value = (0, 0, 0, 1); bs.inputs["Roughness"].default_value = 1.0; bs.inputs["Metallic"].default_value = 0.0
            nt.nodes["VIEW_TEX"].image = vimg
            nt.links.new(nt.nodes["VIEW_TEX"].outputs[0], bs.inputs["Emission Color"]); bs.inputs["Emission Strength"].default_value = 1.0
    for o in [o for o in sc.objects if o.type == "LIGHT"]: bpy.data.objects.remove(o)
    view = os.path.join(S["out"], f"{pid}_view.glb")
    bpy.ops.export_scene.gltf(filepath=view, export_lights=False, export_extras=False, **gltf)
    log(f"{pid} viewer copy: {os.path.getsize(view)/1e6:.1f} MB")


def preview(path, eye, target, lens=24, samples=64, res=(1280, 720)):
    """Cycles still of the built place (before the bake): framing / look check"""
    sc = bpy.context.scene
    cam = bpy.data.cameras.new("preview_cam"); cam.lens = lens
    ob = link(bpy.data.objects.new("preview_cam", cam)); ob.location = eye; aim(ob, target)
    sc.camera = ob; sc.render.engine = "CYCLES"; sc.cycles.samples = samples; sc.cycles.use_denoising = True
    sc.render.resolution_x, sc.render.resolution_y = res; sc.render.filepath = path
    sc.view_settings.view_transform = "AgX"; sc.view_settings.look = "AgX - Medium High Contrast"
    bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(ob)
