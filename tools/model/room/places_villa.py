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
    TX.bay_panorama(K.tex("bay.jpg"))
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
        void=K.mat("place_interior", color=(0, 0, 0), emit=(0.022, 0.025, 0.03), emit_strength=1.0, baked=False),
        bay=K.mat("place_view", color=(0, 0, 0), emit_tex=K.tex("bay.jpg"), emit_strength=1.0, baked=False),
    )
    EL.floor_rect("terrace_floor", R[0] - 0.1, R[1] + 0.1, R[2], R[3] + 0.1, 0.0, M["floor"], uv=0.6)   # runs under the parapets
    # the house: its whitewashed wall behind the terrace (-y), a door and a window with blue shutters, open
    EL.wall_span("house_wall", "-Y", R[2], R[0], R[1], 0.0, 3.4, M["lime"], holes=[(-1.9, -0.9, 0.0, 2.2), (0.9, 2.1, 0.9, 2.2)])
    F = EL.WallFrame("-Y", R[2])
    for (a0, a1, z0, z1, nm) in ((-1.9, -0.9, 0.0, 2.2, "door"), (0.9, 2.1, 0.9, 2.2, "window")):
        F.box(f"{nm}_reveal_l", a0 - 0.02, a0, -0.3, 0.0, z0, z1, M["lime"])
        F.box(f"{nm}_reveal_r", a1, a1 + 0.02, -0.3, 0.0, z0, z1, M["lime"])
        F.box(f"{nm}_reveal_t", a0, a1, -0.3, 0.0, z1, z1 + 0.02, M["lime"])
        F.box(f"{nm}_reveal_b", a0 - 0.02, a1 + 0.02, -0.3, 0.02, z0 - 0.03, z0 + 0.002, M["lime"])  # sill / threshold
        F.box(f"{nm}_dark", a0, a1, -0.3, -0.28, z0, z1, M["void"])                 # the dim interior, seen from the sun
        w = (a1 - a0) / 2
        for s, (b0, b1) in enumerate(((a0 - w + 0.02, a0), (a1, a1 + w - 0.02))):    # shutters folded back
            F.box(f"{nm}_shutter{s}", b0, b1, 0.0, 0.035, z0 + 0.02, z1 - 0.02, M["shutter"], bevel=0.006)
            for k in range(int((z1 - z0) / 0.12)):
                F.box(f"{nm}_shutter{s}_slat{k}", b0 + 0.03, b1 - 0.03, 0.035, 0.045, z0 + 0.08 + k * 0.12, z0 + 0.1 + k * 0.12, M["shutter"])
    K.box("wall_cap", (R[1] - R[0] + 0.1, 0.35, 0.1), (0.0, R[2] - 0.12, 3.45), M["lime"], bevel=0.03)
    # the parapet over the bay along the front, low side walls; the bay all around: a panorama inside the far clip
    K.box("parapet", (R[1] - R[0] + 0.64, 0.32, 0.92), (0.0, R[3] + 0.16, 0.40), M["lime"], bevel=0.03, uv_scale=0.8)   # its bevel below the tiles
    K.box("parapet_cap", (R[1] - R[0] + 0.7, 0.4, 0.06), (0.0, R[3] + 0.16, 0.89), M["lime"], bevel=0.02)
    for sx in (-1, 1):
        K.box(f"side_wall_{sx}", (0.32, R[3] - R[2] + 0.1, 0.92), (sx * (R[1] + 0.16), (R[2] + R[3]) / 2, 0.40), M["lime"], bevel=0.03, uv_scale=0.8)
    EL.panorama("bay_view", (0.0, (R[2] + R[3]) / 2), 30.0, 40.0, 1.6, M["bay"])      # every camera mark sees all of it
    # a vine pergola over the table (the right side), the table for two, bougainvillea on the house wall, a lemon tree
    for k, (px, py) in enumerate(((1.75, -2.75), (3.25, -2.75), (1.75, -0.45), (3.25, -0.45))):
        K.box(f"pergola_post{k}", (0.12, 0.12, 2.5), (px, py, 1.25), M["wood"], bevel=0.01)
    for k in range(6):
        K.box(f"pergola_beam{k}", (1.75, 0.08, 0.12), (2.5, -2.75 + k * 0.46, 2.55), M["wood"], bevel=0.008)
    rnd = random.Random(4)

    def leaves(name, n, place, size, mat):
        """n thin leaves / petals, each turned freely in 3D, joined into one mesh"""
        obs = []
        for k in range(n):
            ob = K.box(f"{name}_{k}", size, (0.0, 0.0, 0.0), mat)
            ob.rotation_euler = (rnd.uniform(-1.2, 1.2), rnd.uniform(-1.2, 1.2), rnd.uniform(0, 6.28)); ob.location = place(k)
            obs.append(ob)
        import bpy
        bpy.context.view_layer.update()
        for ob in obs:
            ob.data.transform(ob.matrix_world); ob.matrix_world = __import__("mathutils").Matrix.Identity(4)
        return EL.join(name, obs)
    leaves("vine", 110, lambda k: (rnd.uniform(1.65, 3.35), rnd.uniform(-2.85, -0.35), 2.63 + rnd.uniform(-0.04, 0.06)), (0.2, 0.16, 0.012), M["leaf"])
    tbl = EL.table("table", 2.5, -1.6, M, w=0.8, d=0.8, h=0.74, top="cloth", legs="iron")
    EL.chair("chair_a", 2.5, -2.15, (0, 1), M, seat="wood", frame="iron")
    EL.chair("chair_b", 2.5, -1.05, (0, -1), M, seat="wood", frame="iron")
    K.cylinder("carafe", 0.05, 0.03, 0.22, (2.35, -1.65, 0.74), M["wine"], seg=20)
    for k, (gx, gy) in enumerate(((2.65, -1.8), (2.65, -1.4))):
        K.cylinder(f"tumbler_{k}", 0.035, 0.03, 0.09, (gx, gy, 0.74), M["glass"], seg=16)
    clumps = []                                                      # bougainvillea: clumps spilling down the house wall's left
    for k in range(28):
        z = rnd.uniform(1.0, 3.4); clumps.append((-3.3 + rnd.random() ** 1.5 * (1.5 if z > 2.3 else 0.9), z))
    def wall_spot(k):
        x, z = clumps[k % len(clumps)]
        return (max(R[0] + 0.05, x + rnd.gauss(0, 0.09)), R[2] + 0.04 + rnd.uniform(0, 0.14), min(3.42, z + rnd.gauss(0, 0.08)))
    leaves("bougainvillea", 170, wall_spot, (0.1, 0.09, 0.008), M["bloom"])
    leaves("bougainvillea_leaf", 80, wall_spot, (0.12, 0.07, 0.008), M["leaf"])
    K.cylinder("lemon_pot", 0.32, 0.26, 0.5, (-2.7, 1.1, 0.0), M["pot"], seg=32)
    K.cylinder("lemon_trunk", 0.04, 0.03, 0.9, (-2.7, 1.1, 0.5), M["wood"], seg=10)
    def crown(k):
        a, r_ = rnd.uniform(0, 6.28), rnd.uniform(0.0, 0.48) ** 0.7
        return (-2.7 + r_ * math.cos(a) * 0.6, 1.1 + r_ * math.sin(a) * 0.6, 1.4 + rnd.uniform(-0.28, 0.3))
    leaves("lemon_leaves", 140, crown, (0.13, 0.06, 0.008), M["leaf"])
    for k in range(9):
        p = crown(k)
        K.cylinder(f"lemon_{k}", 0.035, 0.035, 0.075, (p[0], p[1], p[2] - 0.05), M["lemon"], seg=12)
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
        "chair_a": anchor(2.5, -2.15, 0.46, (0, 1)), "chair_b": anchor(2.5, -1.05, 0.46, (0, -1)),
    }
    engine = {"lamp_key": 120.0, "moon": 2.2, "sky_fill": None}
    previews = [("stand", (0.3, -2.8, 1.6), (0.0, 2.5, 1.0)), ("table", (-0.8, -2.5, 1.8), (2.2, 0.0, 0.7)),
                ("house", (0.5, 1.2, 1.6), (-0.6, -3.0, 1.6))]
    return meta, engine, previews
