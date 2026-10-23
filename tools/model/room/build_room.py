# Afterglow native room: a real-scale night bedroom (Blender, Z-up; exported Y-up).
#   blender -b --python build_room.py            BAKE_RES=2048 BAKE_SAMPLES=96 (env)
# Output: /root/agb/native/room/room_raw.glb + lightmap.png + room.json (lights, bed, camera bounds)
# The post step (tools/model/room/post.mjs) checks the lightmap (emissive, texCoord 1) and writes the scene extras.
import bpy, bmesh, math, os, sys, json, time
import numpy as np
from mathutils import Vector, Matrix
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import textures as TX
OUT = "/root/agb/native/room"; TEX = OUT + "/tex"; os.makedirs(TEX, exist_ok=True)
BAKE_RES = int(os.environ.get("BAKE_RES", "2048")); BAKE_SAMPLES = int(os.environ.get("BAKE_SAMPLES", "96"))
def log(*a): print("[room]", *a, flush=True)
t0 = time.time()
bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene

# ------------------------------------------------------------------ layout (metres)
RX, RY0, RY1, RZ = 2.6, -3.0, 2.45, 2.8          # room: x in [-RX,RX], y in [RY0,RY1], height RZ
BED_C = Vector((0.0, 1.22, 0.0)); MAT_W, MAT_L, MAT_TOP, MAT_H = 1.80, 2.00, 0.60, 0.24
BED_TOP = MAT_TOP + 0.004                         # fitted-sheet surface the characters rest on

# ------------------------------------------------------------------ textures + materials
TX.wood_planks(TEX + "/floor.jpg", TEX + "/floor_n.jpg")
TX.damask_wall(TEX + "/wall.jpg", TEX + "/wall_n.jpg")
TX.fabric(TEX + "/velvet.jpg", TEX + "/velvet_n.jpg", (0.30, 0.04, 0.07), weave=140, sheen=0.5, seed=1)
TX.fabric(TEX + "/sheet.jpg", TEX + "/sheet_n.jpg", (0.42, 0.08, 0.11), weave=200, sheen=0.4, seed=2)
TX.fabric(TEX + "/linen.jpg", TEX + "/linen_n.jpg", (0.78, 0.70, 0.62), weave=160, sheen=0.1, seed=3)
TX.fabric(TEX + "/duvet.jpg", TEX + "/duvet_n.jpg", (0.20, 0.05, 0.07), weave=120, sheen=0.35, seed=4)
TX.fabric(TEX + "/shade.jpg", TEX + "/shade_n.jpg", (0.86, 0.74, 0.58), weave=90, sheen=0.05, seed=5)
TX.rug(TEX + "/rug.jpg"); TX.night_sky(TEX + "/sky.jpg")
log("textures generated")

def image(path, colorspace="sRGB"):
    im = bpy.data.images.load(path, check_existing=True); im.colorspace_settings.name = colorspace; return im
def make_mat(name, albedo=None, normal=None, color=(0.5, 0.5, 0.5), rough=0.6, metal=0.0, emit=None, emit_tex=None, emit_strength=1.0):
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial"); bs = nt.nodes.new("ShaderNodeBsdfPrincipled"); nt.links.new(bs.outputs[0], out.inputs[0])
    bs.inputs["Roughness"].default_value = rough; bs.inputs["Metallic"].default_value = metal
    bs.inputs["Base Color"].default_value = (*color, 1)
    if albedo:
        t = nt.nodes.new("ShaderNodeTexImage"); t.image = image(albedo); nt.links.new(t.outputs[0], bs.inputs["Base Color"])
    if normal:
        tn = nt.nodes.new("ShaderNodeTexImage"); tn.image = image(normal, "Non-Color")
        nm = nt.nodes.new("ShaderNodeNormalMap"); nm.inputs["Strength"].default_value = 0.6
        nt.links.new(tn.outputs[0], nm.inputs["Color"]); nt.links.new(nm.outputs[0], bs.inputs["Normal"])
    if emit is not None or emit_tex:
        bs.inputs["Emission Strength"].default_value = emit_strength
        if emit_tex:
            te = nt.nodes.new("ShaderNodeTexImage"); te.image = image(emit_tex); nt.links.new(te.outputs[0], bs.inputs["Emission Color"])
        else:
            bs.inputs["Emission Color"].default_value = (*emit, 1)
    return m
M = {
    "floor": make_mat("room_floor", TEX + "/floor.jpg", TEX + "/floor_n.jpg", rough=0.42),
    "wall": make_mat("room_wall", TEX + "/wall.jpg", TEX + "/wall_n.jpg", rough=0.85),
    "panel": make_mat("room_panel", color=(0.10, 0.045, 0.04), rough=0.45),
    "ceiling": make_mat("room_ceiling", color=(0.12, 0.07, 0.07), rough=0.9),
    "walnut": make_mat("room_walnut", TEX + "/floor.jpg", TEX + "/floor_n.jpg", rough=0.35),
    "velvet": make_mat("room_velvet", TEX + "/velvet.jpg", TEX + "/velvet_n.jpg", rough=0.7),
    "sheet": make_mat("room_sheet", TEX + "/sheet.jpg", TEX + "/sheet_n.jpg", rough=0.38),
    "linen": make_mat("room_pillow", TEX + "/linen.jpg", TEX + "/linen_n.jpg", rough=0.62),
    "duvet": make_mat("room_duvet", TEX + "/duvet.jpg", TEX + "/duvet_n.jpg", rough=0.5),
    "mattress": make_mat("room_mattress", color=(0.80, 0.76, 0.70), rough=0.8),
    "rug": make_mat("room_rug", TEX + "/rug.jpg", None, rough=0.95),
    "ceramic": make_mat("room_ceramic", color=(0.08, 0.06, 0.07), rough=0.18),
    "brass": make_mat("room_brass", color=(0.75, 0.55, 0.28), rough=0.3, metal=1.0),
    "shade": make_mat("room_lampshade", TEX + "/shade.jpg", None, rough=0.9, emit=(1.0, 0.62, 0.34), emit_strength=2.2),
    "wax": make_mat("room_candle", color=(0.92, 0.86, 0.76), rough=0.5),
    "flame": make_mat("room_flame", color=(1.0, 0.7, 0.3), emit=(1.0, 0.62, 0.25), emit_strength=12.0),
    "sky": make_mat("room_window_sky", color=(0, 0, 0), emit_tex=TEX + "/sky.jpg", emit_strength=1.0),
    "glass_frame": make_mat("room_window_frame", color=(0.06, 0.04, 0.035), rough=0.4),
    "curtain": make_mat("room_curtain", TEX + "/velvet.jpg", TEX + "/velvet_n.jpg", rough=0.75),
    "book1": make_mat("room_book_a", color=(0.25, 0.05, 0.05), rough=0.6),
    "book2": make_mat("room_book_b", color=(0.06, 0.10, 0.16), rough=0.6),
    "bottle": make_mat("room_bottle", color=(0.03, 0.08, 0.04), rough=0.08),
    "art": make_mat("room_art", color=(0.5, 0.5, 0.5), rough=0.8),
}
NO_LIGHTMAP = {"room_lampshade", "room_flame", "room_window_sky"}

# ------------------------------------------------------------------ mesh helpers
def link(ob): sc.collection.objects.link(ob); return ob
def from_bm(name, bm, material, smooth=False):
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    for p in me.polygons: p.use_smooth = smooth
    ob = link(bpy.data.objects.new(name, me)); ob.data.materials.append(material); return ob
def box(name, size, loc, material, bevel=0.0, segs=2, smooth=False, uv_scale=1.0):
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts: v.co = Vector((v.co.x * size[0], v.co.y * size[1], v.co.z * size[2])) + Vector(loc)
    if bevel > 0: bmesh.ops.bevel(bm, geom=bm.verts[:] + bm.edges[:], offset=bevel, segments=segs, affect="EDGES")
    ob = from_bm(name, bm, material, smooth or bevel > 0)
    box_uv(ob, uv_scale); return ob
def box_uv(ob, scale=1.0):
    """world-space triplanar-ish box mapping so tiling textures keep a constant texel density"""
    me = ob.data; uv = me.uv_layers.new(name="UVMap")
    for p in me.polygons:
        n = p.normal; ax = max(range(3), key=lambda i: abs(n[i]))
        for li in p.loop_indices:
            c = ob.matrix_world @ me.vertices[me.loops[li].vertex_index].co
            uv.data[li].uv = ((c.y, c.z) if ax == 0 else (c.x, c.z) if ax == 1 else (c.x, c.y))
            uv.data[li].uv = (uv.data[li].uv[0] * scale, uv.data[li].uv[1] * scale)
def grid(name, sx, sy, nx, ny, material, fn=None, loc=(0, 0, 0), smooth=True, uv_scale=1.0):
    bm = bmesh.new(); bmesh.ops.create_grid(bm, x_segments=nx, y_segments=ny, size=0.5)
    for v in bm.verts:
        x, y = v.co.x * sx, v.co.y * sy; z = fn(x, y) if fn else 0.0
        v.co = Vector((x, y, z)) + Vector(loc)
    ob = from_bm(name, bm, material, smooth)
    uv = ob.data.uv_layers.new(name="UVMap")
    for li, l in enumerate(ob.data.loops):
        c = ob.data.vertices[l.vertex_index].co; uv.data[li].uv = (c.x * uv_scale, c.y * uv_scale)
    return ob
def cylinder(name, r0, r1, h, loc, material, seg=32, cap=True):
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=cap, cap_tris=False, segments=seg, radius1=r0, radius2=r1, depth=h)
    for v in bm.verts: v.co += Vector(loc) + Vector((0, 0, h / 2))
    ob = from_bm(name, bm, material, True); box_uv(ob, 1.0); return ob

# ------------------------------------------------------------------ shell: floor, walls, panels, ceiling
floor = grid("floor", 2 * RX, RY1 - RY0, 1, 1, M["floor"], loc=(0, (RY0 + RY1) / 2, 0), smooth=False, uv_scale=0.8)
for nm, sx, loc, rot in (("wall_back", 2 * RX, (0, RY1, RZ / 2), (math.pi / 2, 0, 0)), ("wall_front", 2 * RX, (0, RY0, RZ / 2), (-math.pi / 2, 0, 0)),
                         ("wall_left", RY1 - RY0, (-RX, (RY0 + RY1) / 2, RZ / 2), (math.pi / 2, 0, -math.pi / 2)),
                         ("wall_right", RY1 - RY0, (RX, (RY0 + RY1) / 2, RZ / 2), (math.pi / 2, 0, math.pi / 2))):
    w = grid(nm, sx, RZ, 1, 1, M["wall"], smooth=False); w.rotation_euler = rot; w.location = loc
    if nm == "wall_back": w.rotation_euler = (-math.pi / 2, math.pi, 0)
    bpy.context.view_layer.update(); bpy.ops.object.select_all(action="DESELECT"); w.select_set(True); bpy.context.view_layer.objects.active = w
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    me = w.data
    if nm == "wall_front":
        for v in me.vertices: pass
    me.uv_layers.remove(me.uv_layers[0]); box_uv(w, 0.9)
ceil = grid("ceiling", 2 * RX, RY1 - RY0, 1, 1, M["ceiling"], loc=(0, (RY0 + RY1) / 2, RZ), smooth=False)
for p in ceil.data.polygons: p.flip()
# wall normals must face into the room
for ob in [o for o in sc.objects if o.name.startswith("wall_")]:
    c = sum((ob.matrix_world @ v.co for v in ob.data.vertices), Vector()) / len(ob.data.vertices)
    inward = Vector((0, (RY0 + RY1) / 2, RZ / 2)) - c
    if ob.data.polygons[0].normal.dot(inward) < 0:
        for p in ob.data.polygons: p.flip()
# wainscoting + chair rail + skirting on all walls
for side, (a, b, fixed, axis) in {"back": (-RX, RX, RY1 - 0.012, "y"), "left": (RY0, RY1, -RX + 0.012, "x"), "right": (RY0, RY1, RX - 0.012, "x"), "front": (-RX, RX, RY0 + 0.012, "y")}.items():
    L = b - a; mid = (a + b) / 2
    def mk(nm, h, z, t, mat_):
        if axis == "y": return box(nm, (L, t, h), (mid, fixed, z), mat_, bevel=0.004)
        return box(nm, (t, L, h), (fixed, mid, z), mat_, bevel=0.004)
    mk(f"skirting_{side}", 0.14, 0.07, 0.022, M["panel"]); mk(f"chairrail_{side}", 0.05, 0.98, 0.03, M["panel"])
    mk(f"wainscot_{side}", 0.84, 0.54, 0.012, M["panel"])
    n = max(2, int(L / 0.9))
    for k in range(n):                                   # raised panel frames
        cpos = a + (k + 0.5) * L / n
        if axis == "y": box(f"panel_{side}_{k}", (L / n - 0.16, 0.012, 0.58), (cpos, fixed + (-0.008 if side == "back" else 0.008), 0.55), M["panel"], bevel=0.006)
        else: box(f"panel_{side}_{k}", (0.012, L / n - 0.16, 0.58), (fixed + (0.008 if side == "left" else -0.008), cpos, 0.55), M["panel"], bevel=0.006)
log("shell built")

# ------------------------------------------------------------------ window with night city + curtains (left wall)
WY, WZ, WW, WH = 0.25, 1.62, 1.3, 1.55
sky = grid("window_sky", 2.2, 1.3, 1, 1, M["sky"], smooth=False)
sky.rotation_euler = (math.pi / 2, 0, -math.pi / 2); sky.location = (-RX - 0.35, WY, WZ)
for nm, size, loc in (("win_top", (0.09, WW + 0.18, 0.09), (-RX + 0.02, WY, WZ + WH / 2)), ("win_bot", (0.14, WW + 0.24, 0.07), (-RX + 0.05, WY, WZ - WH / 2)),
                      ("win_l", (0.09, 0.09, WH), (-RX + 0.02, WY - WW / 2, WZ)), ("win_r", (0.09, 0.09, WH), (-RX + 0.02, WY + WW / 2, WZ)),
                      ("win_mv", (0.05, 0.04, WH), (-RX + 0.02, WY, WZ)), ("win_mh", (0.05, WW, 0.04), (-RX + 0.02, WY, WZ + 0.18))):
    box(nm, size, loc, M["glass_frame"], bevel=0.008)
# cut the window hole: the wall segment behind the window is removed by splitting the left wall into 4 strips
wl = sc.objects["wall_left"]; bpy.data.objects.remove(wl)
for nm, y0, y1, z0, z1 in (("wall_left_a", RY0, WY - WW / 2, 0, RZ), ("wall_left_b", WY + WW / 2, RY1, 0, RZ),
                           ("wall_left_c", WY - WW / 2, WY + WW / 2, 0, WZ - WH / 2), ("wall_left_d", WY - WW / 2, WY + WW / 2, WZ + WH / 2, RZ)):
    w = grid(nm, y1 - y0, z1 - z0, 1, 1, M["wall"], smooth=False)
    w.rotation_euler = (math.pi / 2, 0, math.pi / 2); w.location = (-RX, (y0 + y1) / 2, (z0 + z1) / 2)
    bpy.context.view_layer.update(); bpy.ops.object.select_all(action="DESELECT"); w.select_set(True); bpy.context.view_layer.objects.active = w
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    if w.data.polygons[0].normal.x < 0:
        for p in w.data.polygons: p.flip()
    w.data.uv_layers.remove(w.data.uv_layers[0]); box_uv(w, 0.9)
def curtain(name, y0, y1, gather):
    def fn(x, y):
        return 0.045 * math.sin(x * 2 * math.pi * 7) * (0.6 + 0.4 * (y / 2.8 + 0.5)) + 0.01 * math.sin(x * 61)
    c = grid(name, y1 - y0, 2.45, 48, 24, M["curtain"], fn=fn, uv_scale=1.2)
    c.rotation_euler = (math.pi / 2, 0, math.pi / 2); c.location = (-RX + 0.16, (y0 + y1) / 2, 1.33)
    return c
curtain("curtain_l", WY - WW / 2 - 0.62, WY - WW / 2 + 0.02, 0); curtain("curtain_r", WY + WW / 2 - 0.02, WY + WW / 2 + 0.62, 0)
box("curtain_rod", (0.03, WW + 1.6, 0.03), (-RX + 0.16, WY, 2.6), M["brass"], bevel=0.01)
log("window built")

# ------------------------------------------------------------------ bed
bx, by = BED_C.x, BED_C.y
box("bed_base", (MAT_W + 0.12, MAT_L + 0.12, 0.26), (bx, by, 0.25), M["walnut"], bevel=0.02, segs=3, uv_scale=1.5)
for sx in (-1, 1):
    for sy in (-1, 1):
        box(f"bed_leg_{sx}{sy}", (0.08, 0.08, 0.12), (bx + sx * (MAT_W / 2 - 0.02), by + sy * (MAT_L / 2 - 0.02), 0.06), M["walnut"], bevel=0.01)
mat_ = box("mattress", (MAT_W, MAT_L, MAT_H), (bx, by, MAT_TOP - MAT_H / 2), M["mattress"], bevel=0.05, segs=4)
def sheet_fn(x, y):                                   # gentle wrinkles in the fitted sheet, flat where they lie
    yy = y + by; edge = max(0.0, abs(x) / (MAT_W / 2) - 0.75) * 4
    return 0.0025 * math.sin(x * 9 + yy * 5) * math.sin(yy * 7.3) * (0.4 + edge) + 0.004 * edge * math.sin(yy * 23)
sheet = grid("sheet", MAT_W + 0.02, MAT_L + 0.02, 90, 100, M["sheet"], fn=sheet_fn, loc=(bx, by, BED_TOP), uv_scale=1.6)
# sheet skirt over the mattress sides
for nm, size, loc in (("sheet_side_l", (0.01, MAT_L, MAT_H - 0.02), (bx - MAT_W / 2 - 0.006, by, MAT_TOP - MAT_H / 2)),
                      ("sheet_side_r", (0.01, MAT_L, MAT_H - 0.02), (bx + MAT_W / 2 + 0.006, by, MAT_TOP - MAT_H / 2)),
                      ("sheet_side_f", (MAT_W, 0.01, MAT_H - 0.02), (bx, by - MAT_L / 2 - 0.006, MAT_TOP - MAT_H / 2))):
    box(nm, size, loc, M["sheet"], bevel=0.004, uv_scale=1.6)
# headboard: tufted velvet panel in a walnut frame
HB_Y = by + MAT_L / 2 + 0.07
def tuft_fn(x, y):
    u = (x / 0.16) % 1.0 - 0.5; v = ((y + 0.04) / 0.16) % 1.0 - 0.5
    du, dv = abs(u) - abs(v), abs(u) + abs(v)
    return -0.018 * math.exp(-((u * u + v * v) / 0.012)) + 0.004 * math.cos(min(1.0, dv) * math.pi)
hb = grid("headboard_tufted", MAT_W + 0.06, 1.0, 120, 70, M["velvet"], fn=tuft_fn, uv_scale=1.0)
hb.rotation_euler = (math.pi / 2, 0, 0); hb.location = (bx, HB_Y, MAT_TOP + 0.38)
bpy.context.view_layer.update(); bpy.ops.object.select_all(action="DESELECT"); hb.select_set(True); bpy.context.view_layer.objects.active = hb
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
if hb.data.polygons[0].normal.y > 0:
    for p in hb.data.polygons: p.flip()
box("headboard_back", (MAT_W + 0.2, 0.06, 1.12), (bx, HB_Y + 0.04, MAT_TOP + 0.38), M["walnut"], bevel=0.02)
box("headboard_cap", (MAT_W + 0.26, 0.10, 0.06), (bx, HB_Y + 0.03, MAT_TOP + 0.95), M["walnut"], bevel=0.02)
# pillows: soft superellipsoid cushions
def pillow(name, cx, cy, cz, sx, sy, sz, tilt, material):
    bm = bmesh.new(); bmesh.ops.create_uvsphere(bm, u_segments=40, v_segments=24, radius=1.0)
    for v in bm.verts:
        x, y, z = v.co
        e = 0.35
        def sp(c): return math.copysign(abs(c) ** e, c)
        x, y, z = sp(x) * sx / 2, sp(y) * sy / 2, sp(z) * sz / 2
        z *= 1.0 - 0.35 * (abs(2 * x / sx) ** 3 + abs(2 * y / sy) ** 3)             # thinner at the edges
        z += 0.004 * math.sin(x * 40) * math.sin(y * 35)
        v.co = Vector((x, y, z))
    ob = from_bm(name, bm, material, True)
    ob.rotation_euler = (tilt, 0, 0); ob.location = (cx, cy, cz)
    uv = ob.data.uv_layers.new(name="UVMap")
    for li, l in enumerate(ob.data.loops):
        c = ob.data.vertices[l.vertex_index].co; uv.data[li].uv = (c.x * 1.5, c.y * 1.5)
    return ob
PIL_Y = by + MAT_L / 2 - 0.30
pillow("pillow_l", bx - 0.45, PIL_Y + 0.08, MAT_TOP + 0.11, 0.72, 0.46, 0.20, -1.05, M["linen"])
pillow("pillow_r", bx + 0.45, PIL_Y + 0.08, MAT_TOP + 0.11, 0.72, 0.46, 0.20, -1.05, M["linen"])
pillow("pillow_head", bx, PIL_Y - 0.05, MAT_TOP + 0.065, 0.74, 0.48, 0.13, -0.12, M["sheet"])
log("bed built")

# ------------------------------------------------------------------ duvet: cloth drop over the foot of the bed
duv = grid("duvet", 2.3, 1.15, 46, 24, M["duvet"], fn=lambda x, y: 0.03 * math.sin(x * 4.1) * math.sin(y * 5.3), loc=(bx, by - MAT_L / 2 + 0.42, MAT_TOP + 0.35), uv_scale=1.2)
for ob in (mat_, sheet, sc.objects["bed_base"]):
    ob.modifiers.new("col", "COLLISION"); ob.collision.thickness_outer = 0.01
cl = duv.modifiers.new("cloth", "CLOTH"); cs = cl.settings
cs.quality = 6; cs.mass = 0.45; cs.tension_stiffness = 12; cs.compression_stiffness = 12; cs.bending_stiffness = 0.8
cl.collision_settings.use_self_collision = True; cl.collision_settings.distance_min = 0.006
sc.frame_start, sc.frame_end = 1, 70
cl.point_cache.frame_start, cl.point_cache.frame_end = 1, 70
for f in range(1, 71): sc.frame_set(f)
bpy.ops.object.select_all(action="DESELECT"); duv.select_set(True); bpy.context.view_layer.objects.active = duv
bpy.ops.object.modifier_apply(modifier="cloth")
for ob in (mat_, sheet, sc.objects["bed_base"]):
    for m in list(ob.modifiers): ob.modifiers.remove(m)
sol = duv.modifiers.new("thick", "SOLIDIFY"); sol.thickness = 0.03; sol.offset = 1.0
bpy.ops.object.modifier_apply(modifier="thick")
for p in duv.data.polygons: p.use_smooth = True
sc.frame_set(1)
log(f"duvet simulated ({time.time()-t0:.0f}s)")

# ------------------------------------------------------------------ nightstands, lamps, candles, decor, rug, art
lamps, candles = [], []
for sx in (-1, 1):
    nx = bx + sx * (MAT_W / 2 + 0.42); ny = by + MAT_L / 2 - 0.25
    box(f"nightstand_{sx}", (0.52, 0.42, 0.56), (nx, ny, 0.30), M["walnut"], bevel=0.012, uv_scale=1.5)
    box(f"nightstand_top_{sx}", (0.56, 0.46, 0.03), (nx, ny, 0.595), M["walnut"], bevel=0.008, uv_scale=1.5)
    box(f"drawer_{sx}", (0.44, 0.012, 0.16), (nx, ny - 0.215, 0.44), M["walnut"], bevel=0.006, uv_scale=1.5)
    cylinder(f"knob_{sx}", 0.012, 0.012, 0.02, (nx, ny - 0.235, 0.43), M["brass"], seg=16)
    lx, ly = nx + sx * 0.06, ny + 0.04
    cylinder(f"lamp_base_{sx}", 0.075, 0.05, 0.30, (lx, ly, 0.61), M["ceramic"], seg=40)
    cylinder(f"lamp_neck_{sx}", 0.012, 0.012, 0.12, (lx, ly, 0.91), M["brass"], seg=16)
    cylinder(f"lamp_shade_{sx}", 0.20, 0.13, 0.26, (lx, ly, 0.98), M["shade"], seg=48, cap=False)
    lamps.append(Vector((lx, ly, 1.08)))
# candles on the left nightstand + a pair on the floor by the window
for k, (cx, cy, cz, h) in enumerate(((bx - MAT_W / 2 - 0.30, by + MAT_L / 2 - 0.36, 0.61, 0.10), (bx - MAT_W / 2 - 0.25, by + MAT_L / 2 - 0.42, 0.61, 0.07),
                                     (-RX + 0.45, WY - 0.9, 0.0, 0.22), (-RX + 0.55, WY - 0.75, 0.0, 0.15))):
    cylinder(f"candle_{k}", 0.028, 0.028, h, (cx, cy, cz), M["wax"], seg=24)
    fl = grid(f"flame_{k}", 0.012, 0.03, 1, 1, M["flame"], smooth=False); fl.rotation_euler = (math.pi / 2, 0, 0.6); fl.location = (cx, cy, cz + h + 0.018)
    candles.append(Vector((cx, cy, cz + h + 0.02)))
for k in range(3):                                     # books
    box(f"book_{k}", (0.17, 0.24, 0.035), (bx + MAT_W / 2 + 0.36, by + MAT_L / 2 - 0.36, 0.63 + k * 0.036), M["book1" if k % 2 else "book2"], bevel=0.003)
cylinder("wine_bottle", 0.038, 0.038, 0.24, (bx + MAT_W / 2 + 0.56, by + MAT_L / 2 - 0.12, 0.61), M["bottle"], seg=24)
cylinder("wine_neck", 0.014, 0.014, 0.09, (bx + MAT_W / 2 + 0.56, by + MAT_L / 2 - 0.12, 0.85), M["bottle"], seg=16)
rug = box("rug", (2.8, 2.2, 0.012), (bx, by - 0.65, 0.006), M["rug"], bevel=0.004)
ru = rug.data.uv_layers[0]
for li, l in enumerate(rug.data.loops):
    c = rug.data.vertices[l.vertex_index].co; ru.data[li].uv = ((c.x - bx + 1.4) / 2.8, (c.y - (by - 0.65) + 1.1) / 2.2)
box("art_frame", (1.3, 0.04, 0.62), (bx, RY1 - 0.03, 2.05), M["walnut"], bevel=0.01)
box("art_canvas", (1.18, 0.02, 0.50), (bx, RY1 - 0.055, 2.05), M["art"])
log("furniture + decor built")

# ------------------------------------------------------------------ lights (bake + published for the engine)
LIGHTS = []
def add_light(kind, name, loc, color, power, radius=0.05, rot=None, spot=None):
    ld = bpy.data.lights.new(name, kind); ld.color = color; ld.energy = power
    if kind in ("POINT", "SPOT"): ld.shadow_soft_size = radius
    if kind == "SPOT": ld.spot_size = spot or 1.4; ld.spot_blend = 0.6
    ob = link(bpy.data.objects.new(name, ld)); ob.location = loc
    if rot: ob.rotation_euler = rot
    d = dict(name=name, type=kind.lower(), position=[loc[0], loc[2], -loc[1]], color=list(color), power_w=power, radius=radius)
    if kind == "SPOT":
        dv = ob.matrix_world.to_3x3() @ Vector((0, 0, -1)); d["direction"] = [dv.x, dv.z, -dv.y]; d["outer_rad"] = ld.spot_size / 2
    if kind == "SUN":
        dv = ob.matrix_world.to_3x3() @ Vector((0, 0, -1)); d["direction"] = [dv.x, dv.z, -dv.y]; d.pop("position")
    LIGHTS.append(d); return ob
BAKE_W = {}                                            # bake power (W); export power is converted for the engine below
aim = Vector((bx, by - 0.15, MAT_TOP + 0.1))
for i, p in enumerate(lamps):
    if i == 1:
        rot = (aim - p).to_track_quat("-Z", "Y").to_euler()
        add_light("SPOT", "lamp_key", p, (1.0, 0.66, 0.40), 120.0, 0.09, rot=rot, spot=math.radians(110))
    else:
        add_light("POINT", f"lamp_{i}", p, (1.0, 0.64, 0.36), 32.0, 0.08)
for i, p in enumerate(candles): add_light("POINT", f"candle_{i}", p, (1.0, 0.56, 0.24), 2.2, 0.02)
add_light("SUN", "moon", (-RX - 1, WY, 3), (0.55, 0.66, 1.0), 0.55, rot=(math.radians(52), 0, math.radians(-90)))
add_light("AREA", "window_glow", (-RX - 0.2, WY, WZ), (0.45, 0.55, 0.9), 18.0)
sc.objects["window_glow"].rotation_euler = (0, math.radians(90), 0); sc.objects["window_glow"].data.size = 1.3
LIGHTS[-1]["direction"] = [1, 0, 0]
world = bpy.data.worlds.new("night"); sc.world = world; world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.006, 0.006, 0.012, 1)

# ------------------------------------------------------------------ lightmap UVs (one atlas) + bake full diffuse irradiance
room_meshes = [o for o in sc.objects if o.type == "MESH"]
lit = [o for o in room_meshes if o.data.materials and o.data.materials[0].name not in NO_LIGHTMAP]
for o in lit:
    if "UVLightmap" not in o.data.uv_layers: o.data.uv_layers.new(name="UVLightmap")
    o.data.uv_layers["UVLightmap"].active = True
bpy.ops.object.select_all(action="DESELECT")
for o in lit: o.select_set(True)
bpy.context.view_layer.objects.active = lit[0]
bpy.ops.object.mode_set(mode="EDIT"); bpy.ops.mesh.select_all(action="SELECT")
bpy.ops.uv.lightmap_pack(PREF_CONTEXT="ALL_FACES", PREF_PACK_IN_ONE=True, PREF_NEW_UVLAYER=False, PREF_MARGIN_DIV=0.25)
bpy.ops.object.mode_set(mode="OBJECT")
for o in lit: o.data.uv_layers["UVMap"].active = True; o.data.uv_layers["UVMap"].active_render = True
log(f"lightmap UVs packed for {len(lit)} objects")
lm = bpy.data.images.new("room_lightmap", BAKE_RES, BAKE_RES, alpha=False, float_buffer=True); lm.colorspace_settings.name = "Non-Color"
for o in lit:
    for m in o.data.materials:
        nt = m.node_tree
        if "LM_TEX" in nt.nodes: continue
        t = nt.nodes.new("ShaderNodeTexImage"); t.name = "LM_TEX"; t.image = lm
        uvn = nt.nodes.new("ShaderNodeUVMap"); uvn.uv_map = "UVLightmap"; nt.links.new(uvn.outputs[0], t.inputs[0])
        nt.nodes.active = t
sc.render.engine = "CYCLES"; sc.cycles.device = "CPU"; sc.cycles.samples = BAKE_SAMPLES; sc.cycles.use_denoising = False
sc.render.bake.use_pass_direct = True; sc.render.bake.use_pass_indirect = True; sc.render.bake.use_pass_color = False
sc.render.bake.margin = 4
tb = time.time()
bpy.ops.object.bake(type="DIFFUSE", pass_filter={"DIRECT", "INDIRECT"}, use_clear=True, margin=4)
log(f"lightmap baked {BAKE_RES}px x {BAKE_SAMPLES} spp in {time.time()-tb:.0f}s")
px = np.empty(BAKE_RES * BAKE_RES * 4, np.float32); lm.pixels.foreach_get(px); px = px.reshape(BAKE_RES, BAKE_RES, 4)[..., :3]
# light denoise: 3x3 gaussian weighted by coverage (keeps island edges)
cov = (px.sum(-1) > 1e-6).astype(np.float32)
def blur(a):
    k = np.array([1, 2, 1], np.float32) / 4
    out = sum(k[i] * np.roll(a, i - 1, 0) for i in range(3)); return sum(k[i] * np.roll(out, i - 1, 1) for i in range(3))
den = blur(px * cov[..., None]) / np.maximum(blur(cov), 1e-4)[..., None]
den = np.where(cov[..., None] > 0, den, px)
scale = float(np.percentile(den[cov > 0].max(-1), 99.5)) or 1.0
enc = np.clip(den / scale, 0, 1)
enc = np.where(enc <= 0.0031308, enc * 12.92, 1.055 * enc ** (1 / 2.4) - 0.055)       # sRGB-encoded linear irradiance
from PIL import Image
Image.fromarray(np.flipud(np.clip(enc * 255 + 0.5, 0, 255).astype(np.uint8))).save(OUT + "/lightmap.png", optimize=True)
log(f"lightmap encoded (scale {scale:.3f})")
# swap the float bake target for the encoded lightmap, wired as EMISSIVE on UV1 (engine: room = base x emissive(uv1))
lmimg = bpy.data.images.load(OUT + "/lightmap.png"); lmimg.colorspace_settings.name = "sRGB"
done = set()
for o in lit:
    for m in o.data.materials:
        if m.name in done: continue
        done.add(m.name); nt = m.node_tree
        lmt = nt.nodes["LM_TEX"]; lmt.image = lmimg
        bs = [n for n in nt.nodes if n.type == "BSDF_PRINCIPLED"][0]
        nt.links.new(lmt.outputs[0], bs.inputs["Emission Color"]); bs.inputs["Emission Strength"].default_value = 1.0
bpy.data.images.remove(lm)

# ------------------------------------------------------------------ publish layout for the engine + export
meta = dict(version=1, units="m", up="+Y",
            bedTop=BED_TOP, bed=dict(center=[bx, BED_TOP, -by], width=MAT_W, length=MAT_L, size=[MAT_W, MAT_L], headboard="-Z", headZ=-(by + MAT_L / 2), footZ=-(by - MAT_L / 2)),
            lights=LIGHTS, lightmap=dict(slot="emissiveTexture", texCoord=1, encoding="sRGB", scale=scale, mode="full diffuse irradiance (direct+indirect) from all static lights; room colour = base x lightmap"),
            bedCenter=[bx, BED_TOP, -by], lamp=[lamps[1].x, lamps[1].z, -lamps[1].y],
            cameraBounds=dict(min=[-RX + 0.25, 0.15, -(RY1 - 0.25)], max=[RX - 0.25, RZ - 0.15, -(RY0 + 0.25)]),
            window=dict(center=[-RX, WZ, -WY]))
json.dump(meta, open(OUT + "/room.json", "w"), indent=1)
sc["afterglow"] = json.dumps(meta)
ENGINE = {"lamp_key": 120.0, "lamp_0": 0.55, "moon": 0.45, "window_glow": None}      # engine units (see scene.cpp lightsFromRoomGlb)
for o in [o for o in sc.objects if o.type == "LIGHT"]:
    if o.name.startswith("candle_"): o.data.energy = 0.12
    elif o.name in ENGINE and ENGINE[o.name] is None: bpy.data.objects.remove(o)
    elif o.name in ENGINE: o.data.energy = ENGINE[o.name]
bpy.ops.wm.save_as_mainfile(filepath=OUT + "/room.blend")
bpy.ops.export_scene.gltf(filepath=OUT + "/room_raw.glb", export_format="GLB", use_selection=False, export_yup=True, export_apply=True,
                          export_texcoords=True, export_normals=True, export_tangents=False, export_materials="EXPORT",
                          export_cameras=False, export_lights=True, export_import_convert_lighting_mode="RAW", export_extras=True, export_image_format="AUTO")
log(f"room exported ({time.time()-t0:.0f}s): {os.path.getsize(OUT + '/room_raw.glb')/1e6:.1f} MB")
