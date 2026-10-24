# Story places: Salt and Sun's white villa above the bay (outdoor, by day). Recipes return (meta, engine, previews)
# for build_place.py; anchors follow the engine contract (elements.py, docs/STORY_ASSETS.md).
import math, random
from mathutils import Vector
import kit as K
import textures as TX
import elements as EL
from elements import anchor


def villa_terrace(common_meta):
    """the villa's terrace: whitewashed walls, blue shutters, terracotta underfoot, a low parapet over the bay (the
    standing scenes' backdrop), bougainvillea, a table for two under a vine pergola, a lemon tree"""
    R = (-3.4, 3.4, -3.0, 1.7)                  # the terrace; the parapet runs along y = R[3]
    TX.tiles(K.tex("terracotta.jpg"), K.tex("terracotta_n.jpg"), color=(0.62, 0.36, 0.24), grout=(0.70, 0.64, 0.56), n=5)
    TX.plaster(K.tex("lime.jpg"), K.tex("lime_n.jpg"), color=(0.93, 0.92, 0.89))
    TX.wood_planks(K.tex("wood.jpg"), K.tex("wood_n.jpg"), base=(0.36, 0.26, 0.17))
    TX.sea_day(K.tex("bay.jpg"))
    TX.fabric(K.tex("cloth.jpg"), K.tex("cloth_n.jpg"), (0.92, 0.92, 0.90), weave=120, sheen=0.05, seed=41)
    M = dict(
        floor=K.mat("place_terracotta", K.tex("terracotta.jpg"), K.tex("terracotta_n.jpg"), rough=0.75),
        lime=K.mat("place_lime", K.tex("lime.jpg"), K.tex("lime_n.jpg"), rough=0.9),
        shutter=K.mat("place_shutter", color=(0.10, 0.30, 0.52), rough=0.5),
        frame=K.mat("place_window_frame", color=(0.10, 0.30, 0.52), rough=0.5),
        wood=K.mat("place_wood", K.tex("wood.jpg"), K.tex("wood_n.jpg"), rough=0.6),
        iron=K.mat("place_iron", color=(0.05, 0.05, 0.05), rough=0.5),
        cloth=K.mat("place_cloth", K.tex("cloth.jpg"), K.tex("cloth_n.jpg"), rough=0.85),
        pot=K.mat("place_pot", color=(0.60, 0.34, 0.22), rough=0.7),
        leaf=K.mat("place_leaf", color=(0.16, 0.30, 0.10), rough=0.6),
        bloom=K.mat("place_bougainvillea", color=(0.78, 0.12, 0.42), rough=0.7),
        lemon=K.mat("place_lemon", color=(0.95, 0.80, 0.12), rough=0.5),
        glass=K.mat("place_glass", color=(0.25, 0.30, 0.32), rough=0.1),
        wine=K.mat("place_wine", color=(0.62, 0.55, 0.30), rough=0.1),
        dark=K.mat("place_dark", color=(0.04, 0.04, 0.05), rough=0.6),
        bay=K.mat("place_view", color=(0, 0, 0), emit_tex=K.tex("bay.jpg"), emit_strength=1.0, baked=False),
    )
    EL.floor_rect("terrace_floor", R[0], R[1], R[2], R[3], 0.0, M["floor"], uv=0.6)
    # the house: its whitewashed wall behind the terrace (-y), a door and a window with blue shutters, open
    EL.wall_span("house_wall", "-Y", R[2], R[0], R[1], 0.0, 3.4, M["lime"], holes=[(-1.9, -0.9, 0.0, 2.2), (0.9, 2.1, 0.9, 2.2)])
    F = EL.WallFrame("-Y", R[2])
    for (a0, a1, z0, z1, nm) in ((-1.9, -0.9, 0.0, 2.2, "door"), (0.9, 2.1, 0.9, 2.2, "window")):
        F.box(f"{nm}_reveal_l", a0 - 0.02, a0, -0.3, 0.0, z0, z1, M["lime"])
        F.box(f"{nm}_reveal_r", a1, a1 + 0.02, -0.3, 0.0, z0, z1, M["lime"])
        F.box(f"{nm}_reveal_t", a0, a1, -0.3, 0.0, z1, z1 + 0.02, M["lime"])
        F.box(f"{nm}_dark", a0, a1, -0.3, -0.28, z0, z1, M["dark"])                 # the cool dark interior
        w = (a1 - a0) / 2
        for s, (b0, b1) in enumerate(((a0 - w + 0.02, a0), (a1, a1 + w - 0.02))):    # shutters folded back
            F.box(f"{nm}_shutter{s}", b0, b1, 0.0, 0.035, z0 + 0.02, z1 - 0.02, M["shutter"], bevel=0.006)
            for k in range(int((z1 - z0) / 0.12)):
                F.box(f"{nm}_shutter{s}_slat{k}", b0 + 0.03, b1 - 0.03, 0.035, 0.045, z0 + 0.08 + k * 0.12, z0 + 0.1 + k * 0.12, M["shutter"])
    K.box("wall_cap", (R[1] - R[0] + 0.1, 0.35, 0.1), (0.0, R[2] - 0.12, 3.45), M["lime"], bevel=0.03)
    # the parapet over the bay along the front, low side walls; the bay itself: a card at the horizon
    K.box("parapet", (R[1] - R[0], 0.32, 0.86), (0.0, R[3] + 0.16, 0.43), M["lime"], bevel=0.03, uv_scale=0.8)
    K.box("parapet_cap", (R[1] - R[0] + 0.06, 0.4, 0.06), (0.0, R[3] + 0.16, 0.89), M["lime"], bevel=0.02)
    for sx in (-1, 1):
        K.box(f"side_wall_{sx}", (0.32, R[3] - R[2], 0.86), (sx * (R[1] + 0.16), (R[2] + R[3]) / 2, 0.43), M["lime"], bevel=0.03, uv_scale=0.8)
    sea = K.grid("bay_view", 150.0, 75.0, 1, 1, M["bay"], smooth=False, uv_fit=True)
    sea.rotation_euler = (math.pi / 2, 0, 0); sea.location = (0.0, 37.0, 1.6 + 75.0 * (0.52 - 0.5))   # the horizon at eye level
    # a vine pergola over the table (the right side), the table for two, bougainvillea on the house wall, a lemon tree
    for k, (px, py) in enumerate(((1.0, -2.2), (3.0, -2.2), (1.0, 0.6), (3.0, 0.6))):
        K.box(f"pergola_post{k}", (0.12, 0.12, 2.5), (px, py, 1.25), M["wood"], bevel=0.01)
    for k in range(7):
        K.box(f"pergola_beam{k}", (2.3, 0.08, 0.12), (2.0, -2.2 + k * 0.47, 2.55), M["wood"], bevel=0.008)
    rnd = random.Random(4)
    for k in range(90):                                              # vine leaves on the beams
        x, y = rnd.uniform(0.9, 3.1), rnd.uniform(-2.3, 0.7)
        K.box(f"vine_{k}", (0.22, 0.18, 0.02), (x, y, 2.62 + rnd.uniform(0, 0.05)), M["leaf"], rot_z=rnd.uniform(0, 3.1))
    tbl = EL.table("table", 2.0, -0.7, M, w=0.8, d=0.8, h=0.74, top="cloth", legs="iron")
    EL.chair("chair_a", 2.0, -1.25, (0, 1), M, seat="wood", frame="iron")
    EL.chair("chair_b", 2.0, -0.15, (0, -1), M, seat="wood", frame="iron")
    K.cylinder("carafe", 0.05, 0.03, 0.22, (1.85, -0.75, 0.74), M["wine"], seg=20)
    for k, (gx, gy) in enumerate(((2.15, -0.9), (2.15, -0.5))):
        K.cylinder(f"tumbler_{k}", 0.035, 0.03, 0.09, (gx, gy, 0.74), M["glass"], seg=16)
    for k in range(160):                                             # bougainvillea spilling over the left of the house wall
        x, z = rnd.uniform(-3.3, -2.0), rnd.uniform(1.0, 3.4)
        K.box(f"bougainvillea_{k}", (0.09, 0.05, 0.08), (x, R[2] + 0.06 + rnd.uniform(0, 0.12), z), M["bloom" if k % 3 else "leaf"], rot_z=rnd.uniform(0, 3))
    K.cylinder("lemon_pot", 0.32, 0.26, 0.5, (-2.7, 1.1, 0.0), M["pot"], seg=32)
    K.cylinder("lemon_trunk", 0.04, 0.03, 0.9, (-2.7, 1.1, 0.5), M["wood"], seg=10)
    for k in range(60):
        a, r = rnd.uniform(0, 6.28), rnd.uniform(0.0, 0.5)
        x, y, z = -2.7 + r * math.cos(a), 1.1 + r * math.sin(a), 1.35 + rnd.uniform(-0.3, 0.35)
        K.box(f"lemon_leaf_{k}", (0.14, 0.08, 0.02), (x, y, z), M["leaf"], rot_z=a)
        if k % 6 == 0: K.cylinder(f"lemon_{k}", 0.035, 0.035, 0.07, (x, y, z - 0.05), M["lemon"], seg=12)
    # daylight: the sun from the sea side, high and warm; the sky's fill; a key for the engine from the sun's side
    K.light("SUN", "moon", (0, 30, 20), (1.0, 0.93, 0.82), 4.0, rot=(math.radians(55), 0, math.radians(200)))
    key = K.light("SPOT", "lamp_key", (2.6, 2.8, 3.4), (1.0, 0.9, 0.78), 0.0, 0.3, spot=math.radians(90))
    K.aim(key, (0.0, -0.6, 1.0))
    K.light("AREA", "sky_fill", (0.0, 0.0, 6.0), (0.75, 0.85, 1.0), 300.0, size=8.0)
    meta = common_meta("villa", "day", Vector((2.6, 2.8, 3.4)), Vector((0.0, R[3] + 3.0, 1.6)), (-3.2, -2.8, 0.15), (3.2, 1.55, 3.0), (-1.2, 1.2, -1.4, 0.4))
    meta["anchors"] = {
        "parapet": anchor(0.0, R[3] - 0.3, 0.0, (0, 1)), "parapet_l": anchor(-0.4, R[3] - 0.3, 0.0, (0, 1)),
        "parapet_r": anchor(0.4, R[3] - 0.3, 0.0, (0, 1)), "parapet_top": anchor(0.0, R[3] + 0.16, 0.92, (0, 1)),
        "table": anchor(tbl.x, tbl.y, tbl.z, (0, 1)), "door": anchor(-1.4, R[2] + 0.4, 0.0, (0, 1)),
        "chair_a": anchor(2.0, -1.25, 0.46, (0, 1)), "chair_b": anchor(2.0, -0.15, 0.46, (0, -1)),
    }
    engine = {"lamp_key": 120.0, "moon": 2.2, "sky_fill": None}
    previews = [("stand", (0.3, -2.8, 1.6), (0.0, 2.5, 1.0)), ("table", (-0.8, -2.5, 1.8), (2.2, 0.0, 0.7)),
                ("house", (0.5, 1.2, 1.6), (-0.6, -3.0, 1.6))]
    return meta, engine, previews
