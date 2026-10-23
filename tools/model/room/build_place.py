# Afterglow story places without a bed (Blender 4.2):  AG_PLACE=<id> AG_ROOM_OUT=<dir> blender -b --python build_place.py
#   env: BAKE_RES (2048), BAKE_SAMPLES (96), AG_PREVIEW=1 renders Cycles stills of the empty place before the bake
# Output in AG_ROOM_OUT: <id>_raw.glb, <id>.json (room.json schema), lightmap.png, <id>.blend
#   then: node post.mjs <id>_raw.glb <id>.json <dir>  ->  room.glb / room.json, shipped as locations/<id>/<id>.{glb,json}
#
# Staging contract (the story clips are authored in the bedroom's world; Blender coords here, engine z = -y):
#   stand.idle  face to face along X: her feet ~(-0.38, -0.45), his ~(+0.38, -0.55); the camera comes from -y,
#               so the backdrop is the +y wall. Keep x in [-1.2, 1.2], y in [-1.4, 0.2] clear floor.
#   sit.bed     side by side facing +x, hips at x ~0.70 on a 0.604 m seat whose front edge is x = 0.92,
#               bodies over y 0.70..1.70, ankles at x ~1.07 hanging 0.22-0.28 m up (a 0.14 m dais takes the feet);
#               the camera comes from +x, so the backdrop is the -x wall.
import bpy, math, os, random, sys
from mathutils import Vector
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import kit as K
import textures as TX

PID = os.environ.get("AG_PLACE", "cabin_living")
OUT = os.environ.get("AG_ROOM_OUT", f"/root/agb/native/places/{PID}")
BAKE_RES = int(os.environ.get("BAKE_RES", "2048")); BAKE_SAMPLES = int(os.environ.get("BAKE_SAMPLES", "96"))
SEAT_TOP, SEAT_FRONT, DAIS = 0.604, 0.92, 0.14
STAND = Vector((0.0, -0.5, 0.0))


def E(p):
    """Blender point -> engine [x, y, z]"""
    return [round(p[0], 3), round(p[2], 3), round(-p[1], 3)]


def common_meta(style, mood, lamp, window, cam_min, cam_max, footprint):
    """room.json fields the engine reads; `footprint` = (x0, x1, y0, y1) of the staged action in Blender coords
    (no bed here: published as the bed block, which aims the key light, fits the shadow map and the play orbits)"""
    x0, x1, y0, y1 = footprint
    c = Vector(((x0 + x1) / 2, (y0 + y1) / 2, SEAT_TOP))
    return dict(version=1, units="m", up="+Y", style=style, mood=mood, place=PID, bedTop=SEAT_TOP,
                bed=dict(center=E(c), width=x1 - x0, length=y1 - y0, size=[x1 - x0, y1 - y0],
                         note="no bed: the staged action's footprint (standing marks + the seat)"),
                bedCenter=E(c), lamp=E(lamp), window=dict(center=E(window)),
                cameraBounds=dict(min=[cam_min[0], cam_min[2], -cam_max[1]], max=[cam_max[0], cam_max[2], -cam_min[1]]))


# ------------------------------------------------------------------ furniture
def sofa(name, y0, y1, fabric, wood, throw=None):
    """a deep sofa facing +x on the dais: seat top SEAT_TOP, front edge SEAT_FRONT, back toward -x"""
    base_top = SEAT_TOP - 0.18
    K.box(f"{name}_base", (SEAT_FRONT + 0.02, y1 - y0, base_top - DAIS), ((SEAT_FRONT - 0.02) / 2, (y0 + y1) / 2, DAIS + (base_top - DAIS) / 2), fabric, bevel=0.03, segs=3, uv_scale=1.4)
    K.box(f"{name}_plinth", (SEAT_FRONT - 0.04, y1 - y0 - 0.06, 0.05), ((SEAT_FRONT - 0.02) / 2, (y0 + y1) / 2, DAIS + 0.02), wood, bevel=0.008)
    K.box(f"{name}_back", (0.24, y1 - y0, 0.62), (0.12, (y0 + y1) / 2, base_top + 0.31), fabric, bevel=0.05, segs=4, uv_scale=1.4)
    for k, ya in enumerate((y0 + 0.1, y1 - 0.1)):
        K.box(f"{name}_arm{k}", (SEAT_FRONT + 0.02, 0.2, 0.36), ((SEAT_FRONT - 0.02) / 2, ya, base_top + 0.16), fabric, bevel=0.06, segs=4, uv_scale=1.4)
    n = 2; L = (y1 - y0 - 0.4) / n
    for k in range(n):
        yc = y0 + 0.2 + (k + 0.5) * L
        K.cushion(f"{name}_seat{k}", (0.66, L - 0.02, 0.2), ((SEAT_FRONT + 0.27) / 2 + 0.04, yc, SEAT_TOP - 0.1), fabric, sag=0.12, e=0.25)
        K.cushion(f"{name}_backc{k}", (0.5, L - 0.04, 0.17), (0.3, yc, base_top + 0.38), fabric, tilt=0.0, sag=0.2, e=0.3).rotation_euler = (0, math.radians(-78), 0)
    if throw is not None:   # a wool throw folded over the far arm
        K.box(f"{name}_throw", (0.5, 0.26, 0.07), (SEAT_FRONT * 0.52, y1 - 0.1, base_top + 0.38), throw, bevel=0.03, segs=3, uv_scale=1.6, rot_z=math.radians(4))


def armchair(name, loc, rot_z, fabric, wood):
    x, y = loc
    def P(dx, dy):
        c, s = math.cos(rot_z), math.sin(rot_z); return (x + dx * c - dy * s, y + dx * s + dy * c)
    K.box(f"{name}_base", (0.78, 0.82, 0.26), (*P(0, 0), 0.25), fabric, bevel=0.04, segs=3, rot_z=rot_z, uv_scale=1.4)
    K.box(f"{name}_back", (0.2, 0.82, 0.62), (*P(-0.3, 0), 0.62), fabric, bevel=0.05, segs=3, rot_z=rot_z, uv_scale=1.4)
    for k, dy in enumerate((-0.36, 0.36)):
        K.box(f"{name}_arm{k}", (0.78, 0.13, 0.3), (*P(0, dy), 0.5), fabric, bevel=0.05, segs=3, rot_z=rot_z, uv_scale=1.4)
    K.cushion(f"{name}_seat", (0.56, 0.58, 0.14), (*P(0.08, 0), 0.43), fabric, sag=0.15, e=0.25, rot_z=rot_z)
    for k, (dx, dy) in enumerate(((0.3, 0.33), (0.3, -0.33), (-0.3, 0.33), (-0.3, -0.33))):
        K.box(f"{name}_foot{k}", (0.05, 0.05, 0.12), (*P(dx, dy), 0.06), wood, rot_z=rot_z)


def floor_lamp(name, loc, brass, shade):
    x, y = loc
    K.cylinder(f"{name}_foot", 0.16, 0.14, 0.03, (x, y, 0.0), brass, seg=40)
    K.cylinder(f"{name}_pole", 0.012, 0.012, 1.36, (x, y, 0.03), brass, seg=16)
    K.cylinder(f"{name}_shade", 0.24, 0.17, 0.3, (x, y, 1.33), shade, seg=48, cap=False)
    return Vector((x, y, 1.47))


def bookshelf(name, x_wall, y0, y1, wood, books, h=2.1, depth=0.34, inward=1):
    xc = x_wall + inward * depth / 2
    K.box(f"{name}_side0", (depth, 0.03, h), (xc, y0 + 0.015, h / 2), wood, bevel=0.004)
    K.box(f"{name}_side1", (depth, 0.03, h), (xc, y1 - 0.015, h / 2), wood, bevel=0.004)
    rnd = random.Random(5)
    for k in range(6):
        z = 0.04 + k * (h - 0.06) / 5
        K.box(f"{name}_shelf{k}", (depth, y1 - y0 - 0.06, 0.025), (xc, (y0 + y1) / 2, z), wood, bevel=0.003)
        if k == 5: break
        yb = y0 + 0.05
        while yb < y1 - 0.12:
            w = rnd.uniform(0.025, 0.05); hb = rnd.uniform(0.2, 0.3)
            if rnd.random() < 0.12: yb += rnd.uniform(0.05, 0.15); continue
            K.box(f"{name}_b{k}_{int(yb*1000)}", (rnd.uniform(0.17, 0.24), w, hb), (xc + inward * 0.02, yb + w / 2, z + 0.0125 + hb / 2), books[rnd.randrange(len(books))], bevel=0.002)
            yb += w + 0.002


# ------------------------------------------------------------------ cabin_living: by the fire, snowed in (The Storm)
def cabin_living():
    R = (-3.3, 3.3, -4.3, 3.0); H = 2.75
    TX.wood_planks(K.tex("floor.jpg"), K.tex("floor_n.jpg"))
    TX.log_wall(K.tex("wall.jpg"), K.tex("wall_n.jpg"), knots=0.84)
    TX.fieldstone(K.tex("stone.jpg"), K.tex("stone_n.jpg"))
    TX.fabric(K.tex("wool.jpg"), K.tex("wool_n.jpg"), (0.56, 0.48, 0.38), weave=110, sheen=0.05, seed=11)
    TX.fabric(K.tex("chair.jpg"), K.tex("chair_n.jpg"), (0.36, 0.17, 0.09), weave=90, sheen=0.15, seed=12)
    TX.plaid(K.tex("plaid.jpg"), K.tex("plaid_n.jpg"), (0.40, 0.07, 0.05), (0.10, 0.10, 0.12), (0.80, 0.70, 0.45), seed=13)
    TX.rug(K.tex("rug.jpg"))
    TX.snow_night(K.tex("snow.jpg"))
    TX.flame(K.tex("flame.jpg"))
    TX.fabric(K.tex("shade.jpg"), K.tex("shade_n.jpg"), (0.86, 0.74, 0.58), weave=90, sheen=0.05, seed=5)
    M = dict(
        floor=K.mat("place_floor", K.tex("floor.jpg"), K.tex("floor_n.jpg"), rough=0.5),
        wall=K.mat("place_wall", K.tex("wall.jpg"), K.tex("wall_n.jpg"), rough=0.8),
        ceiling=K.mat("place_ceiling", K.tex("wall.jpg"), K.tex("wall_n.jpg"), rough=0.85),
        beam=K.mat("place_beam", color=(0.17, 0.09, 0.045), rough=0.7),
        stone=K.mat("place_stone", K.tex("stone.jpg"), K.tex("stone_n.jpg"), rough=0.9, normal_strength=0.9),
        soot=K.mat("place_soot", color=(0.025, 0.02, 0.018), rough=0.95),
        wool=K.mat("place_wool", K.tex("wool.jpg"), K.tex("wool_n.jpg"), rough=0.85),
        chair=K.mat("place_chair", K.tex("chair.jpg"), K.tex("chair_n.jpg"), rough=0.6),
        plaid=K.mat("place_plaid", K.tex("plaid.jpg"), K.tex("plaid_n.jpg"), rough=0.85),
        rug=K.mat("place_rug", K.tex("rug.jpg"), rough=0.95),
        wood=K.mat("place_wood", K.tex("floor.jpg"), K.tex("floor_n.jpg"), rough=0.45),
        bark=K.mat("place_bark", color=(0.12, 0.075, 0.045), rough=0.9),
        brass=K.mat("place_brass", color=(0.75, 0.55, 0.28), rough=0.3, metal=1.0),
        iron=K.mat("place_iron", color=(0.05, 0.045, 0.04), rough=0.55, metal=1.0),
        glass=K.mat("place_glass", color=(0.05, 0.03, 0.03), rough=0.06),
        wine=K.mat("place_wine", color=(0.18, 0.01, 0.03), rough=0.1),
        wax=K.mat("place_candle", color=(0.92, 0.86, 0.76), rough=0.5),
        frame=K.mat("place_window_frame", color=(0.10, 0.06, 0.035), rough=0.5),
        book_a=K.mat("place_book_a", color=(0.30, 0.06, 0.05), rough=0.6), book_b=K.mat("place_book_b", color=(0.07, 0.12, 0.16), rough=0.6),
        book_c=K.mat("place_book_c", color=(0.42, 0.33, 0.20), rough=0.6), book_d=K.mat("place_book_d", color=(0.10, 0.16, 0.09), rough=0.6),
        shade=K.mat("place_lampshade", K.tex("shade.jpg"), None, rough=0.9, emit=(1.0, 0.62, 0.34), emit_strength=1.4, baked=False),
        flame=K.mat("place_flame", color=(0, 0, 0), emit_tex=K.tex("flame.jpg"), emit_strength=3.0, baked=False),
        ember=K.mat("place_ember", color=(0.05, 0.02, 0.01), emit=(1.0, 0.32, 0.08), emit_strength=3.0, baked=False),
        candleflame=K.mat("place_candleflame", color=(1.0, 0.7, 0.3), emit=(1.0, 0.62, 0.25), emit_strength=12.0, baked=False),
        snow=K.mat("place_view", color=(0, 0, 0), emit_tex=K.tex("snow.jpg"), emit_strength=1.0, baked=False),
    )
    # shell: log walls, plank floor, beamed ceiling; windows behind both stagings (snow outside)
    WIN_Y = [(-1.75, 1.15), (0.35, 1.15)]            # +y wall: behind the standing couple
    WIN_X = (1.2, 1.5)                               # -x wall: behind the seated couple
    K.floor_and_ceiling(R, H, M["floor"], M["ceiling"])
    K.wall("wall_back", "+Y", R, H, M["wall"], holes=[(c - w / 2, c + w / 2, 0.85, 2.25) for c, w in WIN_Y])
    K.wall("wall_left", "-X", R, H, M["wall"], holes=[(WIN_X[0] - WIN_X[1] / 2, WIN_X[0] + WIN_X[1] / 2, 0.9, 2.2)])
    K.wall("wall_right", "+X", R, H, M["wall"])
    K.wall("wall_front", "-Y", R, H, M["wall"])
    for k, (c, w) in enumerate(WIN_Y):
        K.window(f"win_b{k}", "+Y", R[3], c, 1.55, w, 1.4, M["snow"], M["frame"], cols=2, rows=3, sill_mat=M["wood"])
    K.window("win_l", "-X", R[0], WIN_X[0], 1.55, WIN_X[1], 1.3, M["snow"], M["frame"], cols=3, rows=2, sill_mat=M["wood"])
    for k, y in enumerate((-3.4, -2.05, -0.7, 0.65, 2.0)):
        K.box(f"beam_{k}", (R[1] - R[0], 0.2, 0.24), (0, y, H - 0.12), M["beam"], bevel=0.015, uv_scale=1.2)
    for nm, side_y in (("skirt_back", R[3] - 0.012), ("skirt_front", R[2] + 0.012)):
        K.box(nm, (R[1] - R[0], 0.024, 0.12), (0, side_y, 0.06), M["beam"], bevel=0.004)
    # the hearth on +x, facing the sofa: fieldstone chimney breast, firebox, hearth slab, timber mantel
    FX, FC, FW = 2.72, 1.2, 1.9                       # breast front, centre y, width
    K.box("hearth_breast_l", (R[1] - FX, (FW - 0.9) / 2, H), ((FX + R[1]) / 2, FC - FW / 2 + (FW - 0.9) / 4, H / 2), M["stone"], uv_scale=1.15)
    K.box("hearth_breast_r", (R[1] - FX, (FW - 0.9) / 2, H), ((FX + R[1]) / 2, FC + FW / 2 - (FW - 0.9) / 4, H / 2), M["stone"], uv_scale=1.15)
    K.box("hearth_breast_top", (R[1] - FX, 0.9, H - 0.86), ((FX + R[1]) / 2, FC, 0.86 + (H - 0.86) / 2), M["stone"], uv_scale=1.15)
    K.box("firebox_back", (0.06, 0.9, 0.74), (R[1] - 0.2, FC, 0.49), M["soot"])
    K.box("firebox_floor", (R[1] - FX - 0.2, 0.9, 0.12), ((FX + R[1] - 0.2) / 2, FC, 0.06), M["soot"])
    for k, s in enumerate((-1, 1)):
        K.box(f"firebox_cheek{k}", (R[1] - FX - 0.2, 0.04, 0.74), ((FX + R[1] - 0.2) / 2, FC + s * 0.43, 0.49), M["soot"])
    K.box("hearth_slab", (0.46, FW + 0.3, 0.1), (FX - 0.2, FC, 0.05), M["stone"], bevel=0.015, uv_scale=1.15)
    K.box("mantel", (0.3, FW + 0.24, 0.16), (FX - 0.08, FC, 1.3), M["beam"], bevel=0.02, uv_scale=1.2)
    for k, y in enumerate((-0.18, 0.0, 0.17)):        # three logs, ember bed, flame cards
        K.cylinder(f"log_{k}", 0.065, 0.06, 0.62, (FX + 0.12, FC + y, 0.19 + 0.04 * (k == 1)), M["bark"], seg=14, axis="X").rotation_euler = (0, 0, math.radians(18 * (k - 1)))
    K.box("embers", (0.36, 0.62, 0.05), (FX + 0.16, FC, 0.145), M["ember"], bevel=0.02)
    for k, (dy, h, rz) in enumerate(((-0.14, 0.36, 10), (0.0, 0.48, -6), (0.15, 0.32, 24), (0.05, 0.26, 80))):
        f = K.grid(f"flame_{k}", 0.26, h, 1, 1, M["flame"], smooth=False, uv_fit=True)
        f.rotation_euler = (math.pi / 2, 0, math.radians(90 + rz)); f.location = (FX + 0.16, FC + dy, 0.17 + h / 2)
    for k, (yy, h) in enumerate(((FC - 0.78, 0.2), (FC - 0.66, 0.14), (FC + 0.74, 0.17))):
        K.cylinder(f"candle_{k}", 0.032, 0.032, h, (FX - 0.08, yy, 1.38), M["wax"], seg=24)
        fl = K.grid(f"candleflame_{k}", 0.014, 0.034, 1, 1, M["candleflame"], smooth=False)
        fl.rotation_euler = (math.pi / 2, 0, math.radians(90)); fl.location = (FX - 0.08, yy, 1.38 + h + 0.02)
    for k in range(9):                                 # firewood stacked beside the breast
        r, c = divmod(k, 3)
        K.cylinder(f"firewood_{k}", 0.07, 0.07, 0.5, (2.95 + 0.0 * c, FC + FW / 2 + 0.16 + c * 0.15 + 0.075 * (r % 2), 0.075 + r * 0.13), M["bark"], seg=12, axis="X")
    # the seating nook: a timber dais under a deep sofa facing the fire, a plaid throw
    K.box("dais", (2.25, 2.5, DAIS), (0.5, 1.2, DAIS / 2), M["wood"], bevel=0.012, uv_scale=0.8)
    sofa("sofa", 0.15, 2.25, M["wool"], M["wood"], throw=M["plaid"])
    K.cylinder("side_table", 0.24, 0.24, 0.04, (0.55, 2.62, 0.52), M["wood"], seg=40)
    K.cylinder("side_table_leg", 0.035, 0.05, 0.52, (0.55, 2.62, 0.0), M["wood"], seg=20)
    K.cylinder("wine_bottle", 0.038, 0.038, 0.24, (0.48, 2.66, 0.56), M["glass"], seg=24)
    K.cylinder("wine_neck", 0.014, 0.014, 0.09, (0.48, 2.66, 0.8), M["glass"], seg=16)
    for k, (gx, gy) in enumerate(((0.62, 2.55), (0.66, 2.72))):
        K.cylinder(f"glass_stem_{k}", 0.004, 0.004, 0.09, (gx, gy, 0.56), M["glass"], seg=10)
        K.cylinder(f"glass_bowl_{k}", 0.03, 0.04, 0.08, (gx, gy, 0.65), M["glass"], seg=24, cap=False)
        K.cylinder(f"glass_wine_{k}", 0.028, 0.032, 0.03, (gx, gy, 0.655), M["wine"], seg=24)
    # the standing area: a rug in front of the sofa's end
    rug = K.box("rug", (2.7, 2.0, 0.012), (0.0, -0.75, 0.006), M["rug"], bevel=0.004)
    uv = rug.data.uv_layers[0]
    for li, l in enumerate(rug.data.loops):
        c = rug.data.vertices[l.vertex_index].co; uv.data[li].uv = ((c.x + 1.35) / 2.7, (c.y + 1.75) / 2.0)
    # reading corner by the left window: armchair toward the fire, floor lamp, books
    armchair("armchair", (-2.35, -0.95), math.radians(25), M["chair"], M["wood"])
    lamp0 = floor_lamp("floor_lamp", (-2.85, -0.25), M["brass"], M["shade"])
    bookshelf("shelf", R[0], -3.1, -1.75, M["wood"], [M["book_a"], M["book_b"], M["book_c"], M["book_d"]])
    CH = Vector((0.0, -0.7, 2.08))                   # iron ring chandelier hanging from the beam above the rug
    K.cylinder("chandelier_chain", 0.008, 0.008, H - 0.12 - CH.z - 0.02, (CH.x, CH.y, CH.z + 0.02), M["iron"], seg=8)
    ring = K.cylinder("chandelier_ring", 0.42, 0.42, 0.03, (CH.x, CH.y, CH.z - 0.015), M["iron"], seg=48, cap=False)
    K.cylinder("chandelier_hub", 0.05, 0.03, 0.08, (CH.x, CH.y, CH.z - 0.04), M["iron"], seg=16)
    for k in range(6):
        a = k * math.pi / 3; cx, cy = CH.x + 0.42 * math.cos(a), CH.y + 0.42 * math.sin(a)
        K.cylinder(f"chandelier_cup_{k}", 0.03, 0.04, 0.03, (cx, cy, CH.z), M["iron"], seg=16)
        K.cylinder(f"chandelier_candle_{k}", 0.018, 0.018, 0.11, (cx, cy, CH.z + 0.03), M["wax"], seg=12)
        fl = K.grid(f"chandelier_flame_{k}", 0.012, 0.03, 1, 1, M["candleflame"], smooth=False)
        fl.rotation_euler = (math.pi / 2, 0, a); fl.location = (cx, cy, CH.z + 0.16)
    # lights: the fire leads (key for the engine), the chandelier, the reading lamp, mantel candles, moonlit snow
    fire = Vector((FX + 0.05, FC, 0.42))
    K.light("POINT", "fire_0", tuple(fire), (1.0, 0.52, 0.2), 210.0, 0.22)
    K.light("POINT", "lamp_1", tuple(CH + Vector((0, 0, 0.12))), (1.0, 0.6, 0.3), 45.0, 0.4)
    key = K.light("SPOT", "lamp_key", tuple(fire + Vector((-0.25, 0, 0.25))), (1.0, 0.6, 0.3), 0.0, 0.2, spot=math.radians(120))
    K.aim(key, (0.5, 0.4, 0.95))
    K.light("POINT", "lamp_0", tuple(lamp0), (1.0, 0.64, 0.36), 40.0, 0.08)
    for k, (yy, h) in enumerate(((FC - 0.78, 0.2), (FC + 0.74, 0.17))):
        K.light("POINT", f"candle_{k}", (FX - 0.08, yy, 1.38 + h + 0.03), (1.0, 0.56, 0.24), 2.2, 0.02)
    K.light("SUN", "moon", (0, 4, 4), (0.55, 0.66, 1.0), 0.5, rot=(math.radians(55), 0, math.radians(200)))
    for k, (c, w) in enumerate(WIN_Y):
        K.light("AREA", f"window_glow_{k}", (c, R[3] - 0.15, 1.55), (0.45, 0.55, 0.9), 30.0, rot=(math.radians(90), 0, 0), size=1.1)
    K.light("AREA", "window_glow_l", (R[0] + 0.15, WIN_X[0], 1.55), (0.45, 0.55, 0.9), 30.0, rot=(0, math.radians(90), 0), size=1.3)
    meta = common_meta("cabin", "evening", fire, Vector((R[0], WIN_X[0], 1.55)), (-3.05, -4.05, 0.15), (2.45, 2.75, 2.55), (-0.7, 1.2, -1.3, 1.9))
    engine = {"lamp_key": 120.0, "lamp_0": 0.55, "lamp_1": 0.45, "fire_": 0.7, "candle_": 0.12, "moon": 0.45, "window_glow": None}
    previews = [("stand", (0.35, -4.0, 1.55), (0.0, 0.6, 1.1)), ("sit", (2.35, 0.3, 1.45), (0.4, 1.3, 0.95)), ("room", (-2.9, -3.9, 2.2), (0.6, 0.9, 0.8))]
    return meta, engine, previews


PLACES = {"cabin_living": cabin_living}

if __name__ == "__main__":
    K.init(OUT)
    meta, engine, previews = PLACES[PID]()
    K.log(f"{PID} built ({len([o for o in bpy.context.scene.objects if o.type == 'MESH'])} meshes)")
    if os.environ.get("AG_PREVIEW"):
        for name, eye, target in previews:
            K.preview(os.path.join(OUT, f"preview_{name}.png"), eye, target, samples=int(os.environ.get("AG_PREVIEW_SPP", "48")))
        K.log("previews rendered")
        if os.environ.get("AG_PREVIEW") == "only": sys.exit(0)
    K.bake_and_export(PID, meta, engine, bake_res=BAKE_RES, samples=BAKE_SAMPLES)
