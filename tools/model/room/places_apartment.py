# Story places: the Neighbours apartment building (fourth floor). Recipes return (meta, engine, previews) for
# build_place.py; anchors / doors follow the engine contract (elements.py, docs/STORY_ASSETS.md).
import math
from mathutils import Vector
import kit as K
import textures as TX
import elements as EL
from elements import anchor


def apartment_hall(common_meta):
    """the fourth-floor landing: her door 4A and his 4B face each other across the hall (both open, a lit entry
    behind each), the stairs come up at the back under a window on the night city; her boxes by 4A"""
    H = 2.7
    HX, Y0, Y1 = 1.6, -2.6, 2.6               # the hall: x in [-HX, HX], y in [Y0, Y1]
    SX0, SY1, MZ = 0.1, 4.6, -1.6             # the stair shaft: x in [SX0, HX], y in [1.2, SY1], down to MZ
    DC = 0.3                                   # both doors centred at y = DC
    TX.terrazzo(K.tex("floor.jpg"), K.tex("floor_n.jpg"))
    TX.plaster(K.tex("wall.jpg"), K.tex("wall_n.jpg"), color=(0.80, 0.77, 0.71))
    TX.wood_planks(K.tex("oak.jpg"), K.tex("oak_n.jpg"), base=(0.42, 0.27, 0.14))
    TX.woven(K.tex("mat.jpg"), K.tex("mat_n.jpg"), color=(0.45, 0.33, 0.20))
    TX.city_night(K.tex("city.jpg"))
    for n in ("4A", "4B"): TX.plaque(K.tex(f"plaque_{n}.jpg"), n)
    labels = {"kitchen": "KITCHEN|FRAGILE|SORRY", "books": "BOOKS", "more": "MORE BOOKS", "bath": "BATHROOM"}
    for k, t in labels.items(): TX.cardboard(K.tex(f"box_{k}.jpg"), t)
    M = dict(
        floor=K.mat("place_floor", K.tex("floor.jpg"), K.tex("floor_n.jpg"), rough=0.4),
        wall=K.mat("place_wall", K.tex("wall.jpg"), K.tex("wall_n.jpg"), rough=0.9),
        ceiling=K.mat("place_ceiling", color=(0.82, 0.80, 0.76), rough=0.95),
        trim=K.mat("place_trim", color=(0.86, 0.84, 0.79), rough=0.5),
        door=K.mat("place_door", color=(0.11, 0.21, 0.19), rough=0.42),
        brass=K.mat("place_brass", color=(0.75, 0.55, 0.28), rough=0.3, metal=1.0),
        button=K.mat("place_button", color=(0.9, 0.86, 0.75), rough=0.4),
        iron=K.mat("place_iron", color=(0.04, 0.04, 0.04), rough=0.5, metal=1.0),
        rail=K.mat("place_rail", K.tex("oak.jpg"), K.tex("oak_n.jpg"), rough=0.35),
        tread=K.mat("place_tread", K.tex("oak.jpg"), K.tex("oak_n.jpg"), rough=0.4),
        riser=K.mat("place_riser", color=(0.84, 0.82, 0.77), rough=0.6),
        metal=K.mat("place_mailbox", color=(0.55, 0.53, 0.50), rough=0.35, metal=1.0),
        mat=K.mat("place_doormat", K.tex("mat.jpg"), K.tex("mat_n.jpg"), rough=0.95),
        pot=K.mat("place_pot", color=(0.70, 0.66, 0.60), rough=0.6),
        leaf=K.mat("place_leaf", color=(0.12, 0.26, 0.10), rough=0.6),
        frame=K.mat("place_window_frame", color=(0.86, 0.84, 0.79), rough=0.5),
        art=K.mat("place_art", color=(0.80, 0.78, 0.72), rough=0.8),
        ink=K.mat("place_ink", color=(0.10, 0.10, 0.12), rough=0.8),
        coat=K.mat("place_coat", color=(0.30, 0.22, 0.16), rough=0.9),
        glow=K.mat("place_glow", color=(1.0, 0.9, 0.75), emit=(1.0, 0.78, 0.52), emit_strength=3.0, baked=False),
        bulb=K.mat("place_bulb", color=(1.0, 0.9, 0.7), emit=(1.0, 0.75, 0.45), emit_strength=8.0, baked=False),
        view=K.mat("place_view", color=(0, 0, 0), emit_tex=K.tex("city.jpg"), emit_strength=1.0, baked=False),
    )
    for n in ("4A", "4B"): M[f"plaque_{n}"] = K.mat(f"place_plaque_{n}", K.tex(f"plaque_{n}.jpg"), rough=0.3, metal=0.0)
    for k in labels: M[f"box_{k}"] = K.mat(f"place_box_{k}", K.tex(f"box_{k}.jpg"), rough=0.85)
    # ---- the doors first (their openings cut the side walls)
    leafA, doorA, holeA = EL.door("4a", "-X", -HX, DC, M, hinge=-1, number="4A")
    leafB, doorB, holeB = EL.door("4b", "+X", HX, DC, M, hinge=1, number="4B")
    # ---- floors, ceiling, walls of the hall and the stair shaft
    EL.floor_rect("floor_a", -HX, HX, Y0, 1.2, 0.0, M["floor"])
    EL.floor_rect("floor_b", -HX, SX0, 1.2, Y1, 0.0, M["floor"])
    EL.floor_rect("floor_mid", SX0, HX, 3.8, SY1, MZ, M["floor"])
    EL.floor_rect("ceiling_a", -HX, HX, Y0, Y1, H, M["ceiling"], up=False)
    EL.floor_rect("ceiling_b", SX0, HX, Y1, SY1, H, M["ceiling"], up=False)
    EL.wall_span("wall_left", "-X", -HX, Y0, Y1, 0.0, H, M["wall"], holes=[holeA])
    EL.wall_span("wall_right", "+X", HX, Y0, 1.2, 0.0, H, M["wall"], holes=[holeB])
    EL.wall_span("wall_shaft_right", "+X", HX, 1.2, SY1, MZ, H, M["wall"])
    EL.wall_span("wall_front", "-Y", Y0, -HX, HX, 0.0, H, M["wall"])
    EL.wall_span("wall_back", "+Y", Y1, -HX, SX0, 0.0, H, M["wall"])
    EL.wall_span("wall_shaft_inner_low", "-X", SX0, 1.2, Y1, MZ, 0.0, M["wall"])
    EL.wall_span("wall_shaft_inner", "-X", SX0, Y1, SY1, MZ, H, M["wall"])
    WIN = (0.85, 1.15, 0.05, 2.15)            # centre x, width, z0, z1 (above the half landing)
    EL.wall_span("wall_shaft_end", "+Y", SY1, SX0, HX, MZ, H, M["wall"], holes=[(WIN[0] - WIN[1] / 2, WIN[0] + WIN[1] / 2, WIN[2], WIN[3])])
    K.window("win_stair", "+Y", SY1, WIN[0], (WIN[2] + WIN[3]) / 2, WIN[1], WIN[3] - WIN[2], M["view"], M["frame"], cols=2, rows=3, sky_dist=2.5, sky_scale=(3.2, 1.8))
    for nm, side, fixed, a0, a1, z0 in (("skirt_l", "-X", -HX, Y0, Y1, 0.0), ("skirt_r", "+X", HX, Y0, 1.2, 0.0), ("skirt_f", "-Y", Y0, -HX, HX, 0.0),
                                       ("skirt_b", "+Y", Y1, -HX, SX0, 0.0), ("skirt_mid", "+X", HX, 3.8, SY1, MZ)):
        EL.skirting(nm, side, fixed, a0, a1, M, z0=z0)
    # ---- the stairs down, a balustrade over the drop, a wall rail along the flight
    y_end, z_end = EL.stair_flight("stairs", SX0 + 0.02, HX - 0.02, 1.2, 10, 0.16, 0.26, M)
    EL.balustrade("balustrade", (SX0 + 0.02, 1.2), (SX0 + 0.02, Y1), M)
    EL.balustrade("handrail", (HX - 0.06, 1.25), (HX - 0.06, y_end), M, spacing=1.2, z0=0.0, z1=z_end)
    # ---- entries behind the doors: hers bare with boxes, his lived-in (lit: an open door shows a real home)
    for side, x0, x1 in (("A", -3.3, -HX - 0.12), ("B", HX + 0.12, 3.3)):
        R = (x0, x1, DC - 1.1, DC + 1.1)
        EL.floor_rect(f"entry{side}_floor", x0, x1, R[2], R[3], 0.0, M["floor"])
        EL.floor_rect(f"entry{side}_ceiling", x0, x1, R[2], R[3], H, M["ceiling"], up=False)
        for nm, s_, fx, a0, a1 in ((f"entry{side}_far", "-X" if side == "A" else "+X", x0 if side == "A" else x1, R[2], R[3]),
                                   (f"entry{side}_s", "-Y", R[2], x0, x1), (f"entry{side}_n", "+Y", R[3], x0, x1)):
            EL.wall_span(nm, s_, fx, a0, a1, 0.0, H, M["wall"])
        near = x1 if side == "A" else x0
        EL.wall_span(f"entry{side}_near", "+X" if side == "A" else "-X", near, R[2], R[3], 0.0, H, M["wall"], holes=[holeA if side == "A" else holeB])
    EL.boxes("entryA_boxes", (-2.75, DC + 0.6), M, [(0, 0, 0, 0.5, 0.4, 0.38, 6, "books"), (0.02, 0.01, 0.38, 0.46, 0.38, 0.34, -4, "more"),
                                                  (0.55, -0.1, 0, 0.45, 0.42, 0.4, 12, "bath")])
    bulbA = Vector((-2.5, DC, H - 0.45))
    K.cylinder("entryA_cord", 0.005, 0.005, 0.45, tuple(bulbA), M["iron"], seg=6)
    K.cylinder("entryA_bulb", 0.03, 0.03, 0.06, tuple(bulbA - Vector((0, 0, 0.03))), M["bulb"], seg=16)
    K.box("entryB_art_frame", (0.04, 0.72, 0.52), (3.28, DC + 0.1, 1.55), M["coat"], bevel=0.006)
    K.box("entryB_art", (0.02, 0.62, 0.42), (3.26, DC + 0.1, 1.55), M["art"])
    for k in range(5):                                                             # an architect's sketch: ink lines
        K.box(f"entryB_ink{k}", (0.004, 0.5 - 0.06 * k, 0.006), (3.248, DC + 0.1, 1.42 + 0.06 * k), M["ink"])
    K.cylinder("entryB_rack", 0.02, 0.02, 1.75, (2.15, DC + 0.85, 0.0), M["iron"], seg=10)
    K.box("entryB_coat", (0.22, 0.12, 0.7), (2.15, DC + 0.78, 1.25), M["coat"], bevel=0.04)
    lampB = EL.sconce("entryB_sconce", "+Y", DC + 1.1, 2.6, 1.75, M)
    # ---- the landing: her boxes by 4A (one split: KITCHEN FRAGILE SORRY), doormats, bells, sconces, mailboxes, a plant
    EL.boxes("boxes", (-1.25, -1.0), M, [(0, 0, 0, 0.5, 0.42, 0.36, 4, "books"), (0.03, 0.02, 0.36, 0.48, 0.4, 0.32, -6, "more"),
                                          (-0.02, -0.56, 0, 0.46, 0.4, 0.4, 10, "bath"), (0.42, -0.3, 0, 0.44, 0.38, 0.3, 28, "kitchen")])
    EL.boxes("boxes_stair", (-1.27, 1.42), M, [(0, 0, 0, 0.5, 0.42, 0.38, -8, "books"), (0.01, 0.0, 0.38, 0.46, 0.4, 0.34, 5, "more")])
    EL.doormat("mat_4a", "-X", -HX, DC, M)
    EL.doormat("mat_4b", "+X", HX, DC, M)
    EL.bell("bell_4a", "-X", -HX, DC + 0.6, M)
    EL.bell("bell_4b", "+X", HX, DC - 0.6, M)
    sA = EL.sconce("sconce_4a", "-X", -HX, DC - 0.85, 1.85, M)
    sB = EL.sconce("sconce_4b", "+X", HX, DC + 0.85, 1.85, M)
    EL.mailboxes("mail", "+Y", Y1, -0.8, M)
    EL.plant("plant", -1.3, 2.3, M, tall=1.2)
    dome = Vector((0.0, -0.3, H - 0.06))
    K.cylinder("ceiling_dome", 0.22, 0.16, 0.06, tuple(dome), M["glow"], seg=32)
    stairlamp = Vector((0.85, 3.4, H - 0.5))
    K.cylinder("stair_cord", 0.005, 0.005, 0.5, tuple(stairlamp), M["iron"], seg=6)
    K.cylinder("stair_shade", 0.16, 0.1, 0.12, tuple(stairlamp - Vector((0, 0, 0.12))), M["glow"], seg=24)
    # ---- light
    key = K.light("SPOT", "lamp_key", tuple(dome - Vector((0, 0, 0.08))), (1.0, 0.82, 0.62), 0.0, 0.2, spot=math.radians(130))
    K.aim(key, (0.0, -0.4, 0.0))
    K.light("AREA", "ceiling_glow", tuple(dome - Vector((0, 0, 0.05))), (1.0, 0.84, 0.66), 70.0, size=0.4)
    K.light("POINT", "lamp_0", tuple(sA), (1.0, 0.74, 0.5), 22.0, 0.08)
    K.light("POINT", "lamp_1", tuple(sB), (1.0, 0.74, 0.5), 22.0, 0.08)
    K.light("POINT", "lamp_2", tuple(bulbA - Vector((0, 0, 0.05))), (1.0, 0.72, 0.45), 30.0, 0.03)
    K.light("POINT", "lamp_3", tuple(lampB), (1.0, 0.74, 0.5), 26.0, 0.08)
    K.light("POINT", "lamp_4", tuple(stairlamp - Vector((0, 0, 0.15))), (1.0, 0.76, 0.52), 28.0, 0.1)
    K.light("SUN", "moon", (0, 6, 4), (0.55, 0.62, 0.9), 0.15, rot=(math.radians(60), 0, math.radians(180)))
    K.light("AREA", "window_glow", (WIN[0], SY1 - 0.2, (WIN[2] + WIN[3]) / 2), (0.55, 0.62, 0.95), 25.0, rot=(math.radians(90), 0, 0), size=1.1)
    meta = common_meta("apartment", "evening", dome, Vector((WIN[0], SY1, 1.1)), (-1.45, -2.45, 0.15), (1.45, 2.45, 2.5), (-1.0, 1.0, -1.3, 0.8))
    meta["doors"] = [doorA, doorB]
    meta["anchors"] = {
        "door_4a_out": anchor(-HX + 0.55, DC, 0.0, (-1, 0)), "door_4a_in": anchor(-HX - 0.12 - 0.55, DC, 0.0, (1, 0)),
        "door_4b_out": anchor(HX - 0.55, DC, 0.0, (1, 0)), "door_4b_in": anchor(HX + 0.12 + 0.55, DC, 0.0, (-1, 0)),
        "bell_4a": anchor(-HX + 0.016, DC + 0.6, 1.32, (1, 0)), "bell_4b": anchor(HX - 0.016, DC - 0.6, 1.32, (-1, 0)),
        "stairs_top": anchor(0.85, 1.4, 0.0, (0, -1)), "landing": anchor(0.0, -0.5, 0.0, (0, -1)),
        "boxes": anchor(-1.22, -0.98, 0.68, (1, 0)), "floor_spill": anchor(-0.85, -1.15, 0.0, (1, 0)),
        "mailboxes": anchor(-0.8, Y1 - 0.2, 1.1, (0, 1)), "entry_4a_boxes": anchor(-2.75, DC + 0.6, 0.72, (1, 0)),
    }
    engine = {"lamp_key": 120.0, "lamp_0": 0.38, "lamp_1": 0.38, "lamp_2": 0.3, "lamp_3": 0.3, "lamp_4": 0.3, "moon": 0.12,
              "ceiling_glow": None, "window_glow": None}
    previews = [("stand", (0.25, -2.4, 1.6), (0.0, 1.0, 1.0)), ("doors", (0.0, -2.3, 1.5), (-1.0, 0.6, 1.0)),
                ("stairs", (-1.2, -0.5, 1.7), (1.0, 3.6, -0.4))]
    return meta, engine, previews


def apartment_living(common_meta, sofa, bookshelf):
    """four-B, the living room: an architect's flat — drawings pinned up, a piano under the window, a drafting table,
    the city outside two big windows; a deep sofa on a low platform (sit.bed: the 0.604 seat), a dining table and a
    galley counter for the plates and the wine"""
    import city
    H = 2.7
    R = (-3.0, 3.2, -3.4, 3.0)
    SEAT_FRONT, DAIS = 0.92, 0.14
    TX.wood_planks(K.tex("floor.jpg"), K.tex("floor_n.jpg"), base=(0.40, 0.27, 0.16))
    TX.plaster(K.tex("wall.jpg"), K.tex("wall_n.jpg"), color=(0.84, 0.82, 0.78))
    TX.fabric(K.tex("sofa.jpg"), K.tex("sofa_n.jpg"), (0.36, 0.40, 0.44), weave=120, sheen=0.08, seed=21)
    TX.rug(K.tex("rug.jpg"))
    for k in range(4): TX.sketch(K.tex(f"sketch{k}.jpg"), seed=k + 1)
    TX.fabric(K.tex("shade.jpg"), K.tex("shade_n.jpg"), (0.9, 0.84, 0.72), weave=90, sheen=0.05, seed=5)
    M = dict(
        floor=K.mat("place_floor", K.tex("floor.jpg"), K.tex("floor_n.jpg"), rough=0.45),
        wall=K.mat("place_wall", K.tex("wall.jpg"), K.tex("wall_n.jpg"), rough=0.9),
        ceiling=K.mat("place_ceiling", color=(0.86, 0.85, 0.82), rough=0.95),
        trim=K.mat("place_trim", color=(0.88, 0.87, 0.84), rough=0.5),
        wood=K.mat("place_wood", K.tex("floor.jpg"), K.tex("floor_n.jpg"), rough=0.4),
        sofa=K.mat("place_sofa", K.tex("sofa.jpg"), K.tex("sofa_n.jpg"), rough=0.85),
        rug=K.mat("place_rug", K.tex("rug.jpg"), rough=0.95),
        iron=K.mat("place_iron", color=(0.05, 0.05, 0.05), rough=0.5, metal=1.0),
        brass=K.mat("place_brass", color=(0.75, 0.55, 0.28), rough=0.3, metal=1.0),
        cabinet=K.mat("place_cabinet", color=(0.86, 0.85, 0.82), rough=0.45),
        worktop=K.mat("place_worktop", K.tex("floor.jpg"), K.tex("floor_n.jpg"), rough=0.35),
        lacquer=K.mat("place_lacquer", color=(0.02, 0.02, 0.025), rough=0.15),
        ivory=K.mat("place_ivory", color=(0.9, 0.88, 0.82), rough=0.3),
        frame=K.mat("place_window_frame", color=(0.12, 0.12, 0.13), rough=0.4),
        book_a=K.mat("place_book_a", color=(0.30, 0.06, 0.05), rough=0.6), book_b=K.mat("place_book_b", color=(0.07, 0.12, 0.16), rough=0.6),
        book_c=K.mat("place_book_c", color=(0.62, 0.55, 0.40), rough=0.6), book_d=K.mat("place_book_d", color=(0.10, 0.16, 0.09), rough=0.6),
        board=K.mat("place_board", color=(0.80, 0.80, 0.78), rough=0.7),
        shade=K.mat("place_lampshade", K.tex("shade.jpg"), None, rough=0.9, emit=(1.0, 0.66, 0.36), emit_strength=1.3, baked=False),
        glow=K.mat("place_glow", color=(1.0, 0.9, 0.75), emit=(1.0, 0.78, 0.52), emit_strength=2.5, baked=False),
    )
    sk = [K.mat(f"place_sketch{k}", K.tex(f"sketch{k}.jpg"), rough=0.85) for k in range(4)]
    WINS = [(-1.45, 1.6), (0.75, 1.6)]
    WZ = (0.9, 2.45)
    K.floor_and_ceiling(R, H, M["floor"], M["ceiling"], floor_uv=0.7)
    K.wall("wall_back", "+Y", R, H, M["wall"], holes=[(c - w / 2, c + w / 2, WZ[0], WZ[1]) for c, w in WINS])
    K.wall("wall_left", "-X", R, H, M["wall"])
    K.wall("wall_right", "+X", R, H, M["wall"])
    K.wall("wall_front", "-Y", R, H, M["wall"])
    for k, (c, w) in enumerate(WINS):
        K.window(f"win_{k}", "+Y", R[3], c, (WZ[0] + WZ[1]) / 2, w, WZ[1] - WZ[0], None, M["frame"], cols=2, rows=2, sill_mat=M["trim"])
    for nm, side, fixed, a0, a1 in (("skirt_l", "-X", R[0], R[2], R[3]), ("skirt_r", "+X", R[1], R[2], R[3]), ("skirt_f", "-Y", R[2], R[0], R[1]), ("skirt_b", "+Y", R[3], R[0], R[1])):
        EL.skirting(nm, side, fixed, a0, a1, M)
    city.build_city((-0.35, R[3] + 0.2, 0.0), (0, 1), drop=12.0)
    # the lounge: a low timber platform, the sofa facing +x (its back to the drawings wall), a side table
    K.box("platform", (2.3, 2.6, DAIS), (0.45, 1.25, DAIS / 2), M["wood"], bevel=0.01, uv_scale=0.8)
    sofa("sofa", 0.15, 2.25, M["sofa"], M["wood"])
    side = EL.table("side_table", 0.55, 2.52, M, w=0.42, d=0.42, h=0.52, top="wood", legs="iron")
    K.box("rug", (2.6, 1.9, 0.012), (-0.1, -0.6, 0.006), M["rug"], bevel=0.004)
    rug = bpy_obj("rug")
    uv = rug.data.uv_layers[0]
    for li, l in enumerate(rug.data.loops):
        c = rug.data.vertices[l.vertex_index].co; uv.data[li].uv = ((c.x + 1.4) / 2.6, (c.y + 1.55) / 1.9)
    # the architect: drawings pinned up behind the sofa, a drafting table under the right window, books
    EL.frames_on_wall("drawings", "-X", R[0], -0.6, 2.4, 1.2, 2.3, M, sk, cols=4, rows=2)
    bookshelf("shelf", R[0], -2.55, -0.95, M["wood"], [M["book_a"], M["book_b"], M["book_c"], M["book_d"]])
    board = K.box("drafting_top", (1.1, 0.75, 0.025), (0.0, 0.0, 0.0), M["board"], bevel=0.004)   # built at the origin, then tilted
    board.rotation_euler = (math.radians(-14), 0, 0); board.location = (1.95, 2.45, 0.95)
    EL.table("drafting_base", 1.95, 2.45, M, w=1.0, d=0.65, h=0.86, top="wood", legs="iron")
    lampD = Vector((2.4, 2.75, 1.35))
    K.cylinder("desklamp_arm", 0.012, 0.012, 0.5, (2.4, 2.75, 0.86), M["iron"], seg=8)
    K.cylinder("desklamp_head", 0.08, 0.05, 0.1, tuple(lampD), M["glow"], seg=20)
    EL.piano("piano", -1.45, R[3] - 0.32, (0, -1), M)
    # the kitchen along the front wall, the dining table and two chairs by it
    ctr = EL.counter("galley", -2.9, -0.8, R[2], 0.62, M, wall_side=-1)
    tbl = EL.table("dining", 1.95, -2.15, M, w=1.3, d=0.82, top="wood", legs="iron")
    EL.chair("chair_a", 1.95, -2.75, (0, 1), M)
    EL.chair("chair_b", 1.95, -1.55, (0, -1), M)
    pend = Vector((1.95, -2.15, 1.65))
    K.cylinder("pendant_cord", 0.004, 0.004, H - pend.z - 0.2, (pend.x, pend.y, pend.z + 0.2), M["iron"], seg=6)
    K.cylinder("pendant", 0.24, 0.12, 0.2, tuple(pend), M["shade"], seg=36, cap=False)
    fl = Vector((-0.45, 2.55, 1.5))
    K.cylinder("floorlamp_pole", 0.012, 0.012, 1.45, (fl.x, fl.y, 0.0), M["brass"], seg=12)
    K.cylinder("floorlamp_shade", 0.22, 0.16, 0.28, (fl.x, fl.y, 1.38), M["shade"], seg=40, cap=False)
    # light: the key over the lounge, the floor lamp, the dining pendant, the desk lamp, the city at the windows
    key = K.light("SPOT", "lamp_key", (0.3, 0.2, H - 0.05), (1.0, 0.78, 0.55), 0.0, 0.2, spot=math.radians(120))
    K.aim(key, (0.4, 0.6, 0.5))
    K.light("AREA", "ceiling_wash", (0.2, 0.0, H - 0.05), (1.0, 0.82, 0.62), 60.0, size=1.2)
    K.light("POINT", "lamp_0", tuple(fl), (1.0, 0.68, 0.40), 40.0, 0.1)
    K.light("POINT", "lamp_1", tuple(pend - Vector((0, 0, 0.1))), (1.0, 0.72, 0.46), 30.0, 0.1)
    K.light("POINT", "lamp_2", tuple(lampD - Vector((0, 0, 0.06))), (1.0, 0.78, 0.55), 14.0, 0.05)
    for k, (c, w) in enumerate(WINS):
        K.light("AREA", f"window_glow_{k}", (c, R[3] - 0.15, (WZ[0] + WZ[1]) / 2), (0.5, 0.58, 0.9), 22.0, rot=(math.radians(90), 0, 0), size=1.4)
    K.light("SUN", "moon", (0, 8, 6), (0.5, 0.58, 0.9), 0.2, rot=(math.radians(62), 0, math.radians(180)))
    meta = common_meta("apartment", "evening", Vector((0.3, 0.2, H - 0.05)), Vector((-0.35, R[3], 1.7)), (-2.75, -3.15, 0.15), (2.95, 2.75, 2.5), (-1.0, 1.6, -1.3, 2.3))
    meta["anchors"] = {
        "table": anchor(tbl.x, tbl.y, tbl.z, (0, 1)), "table_l": anchor(tbl.x, tbl.y - 0.22, tbl.z, (0, 1)),
        "table_r": anchor(tbl.x, tbl.y + 0.22, tbl.z, (0, -1)), "counter": anchor(ctr.x, ctr.y, ctr.z, (0, -1)),
        "side_table": anchor(side.x, side.y, side.z, (1, 0)), "piano": anchor(-1.45, R[3] - 0.32 - 0.9, 0.0, (0, 1)),
        "window": anchor(0.75, R[3] - 0.6, 0.0, (0, 1)), "sofa_l": anchor(0.7, 0.95, SEAT_FRONT * 0 + 0.604, (1, 0)),
        "sofa_r": anchor(0.7, 1.45, 0.604, (1, 0)), "drafting": anchor(1.95, 2.0, 0.0, (0, 1)),
    }
    engine = {"lamp_key": 120.0, "lamp_0": 0.5, "lamp_1": 0.4, "lamp_2": 0.25, "moon": 0.2, "ceiling_wash": None, "window_glow": None}
    previews = [("stand", (0.3, -3.1, 1.55), (-0.2, 1.2, 1.1)), ("sit", (2.7, 1.1, 1.45), (0.3, 1.2, 0.95)),
                ("window", (1.2, -1.5, 1.6), (-0.4, 6.0, 1.0))]
    return meta, engine, previews


def bpy_obj(name):
    import bpy
    return bpy.data.objects[name]


def apartment_balcony(common_meta):
    """the balcony of four-B over the city at night: barely wide enough for two, iron railings, string lights, a
    bistro table; French doors stand open on the lit flat behind (the camera can stand inside and look out)"""
    import city
    H = 2.6
    BX, BY0, BY1 = 1.7, -1.3, 0.35            # the balcony: x in [-BX, BX], y in [BY0 (the facade), BY1 (the rail)]
    IR = (-2.6, 2.6, -4.6, BY0 - 0.14)          # the flat behind the facade
    DOOR = (-1.0, 1.0, 2.3)                     # the French doors' opening: x0, x1, height
    TX.tiles(K.tex("pavers.jpg"), K.tex("pavers_n.jpg"), color=(0.52, 0.47, 0.42), grout=(0.32, 0.30, 0.28), n=4)
    TX.plaster(K.tex("render.jpg"), K.tex("render_n.jpg"), color=(0.62, 0.58, 0.54))
    TX.plaster(K.tex("wall.jpg"), K.tex("wall_n.jpg"), color=(0.84, 0.82, 0.78))
    TX.wood_planks(K.tex("floor.jpg"), K.tex("floor_n.jpg"), base=(0.40, 0.27, 0.16))
    TX.fabric(K.tex("sofa.jpg"), K.tex("sofa_n.jpg"), (0.36, 0.40, 0.44), weave=120, sheen=0.08, seed=21)
    M = dict(
        pavers=K.mat("place_pavers", K.tex("pavers.jpg"), K.tex("pavers_n.jpg"), rough=0.7),
        render=K.mat("place_render", K.tex("render.jpg"), K.tex("render_n.jpg"), rough=0.9),
        wall=K.mat("place_wall", K.tex("wall.jpg"), K.tex("wall_n.jpg"), rough=0.9),
        floor=K.mat("place_floor", K.tex("floor.jpg"), K.tex("floor_n.jpg"), rough=0.45),
        ceiling=K.mat("place_ceiling", color=(0.86, 0.85, 0.82), rough=0.95),
        soffit=K.mat("place_soffit", color=(0.55, 0.52, 0.49), rough=0.9),
        trim=K.mat("place_trim", color=(0.15, 0.15, 0.16), rough=0.45),
        iron=K.mat("place_iron", color=(0.05, 0.05, 0.05), rough=0.5),
        rail=K.mat("place_rail", color=(0.06, 0.06, 0.06), rough=0.45),
        glass=K.mat("place_glass", color=(0.18, 0.22, 0.26), rough=0.08),
        sofa=K.mat("place_sofa", K.tex("sofa.jpg"), K.tex("sofa_n.jpg"), rough=0.85),
        pot=K.mat("place_pot", color=(0.55, 0.30, 0.20), rough=0.7),
        leaf=K.mat("place_leaf", color=(0.10, 0.22, 0.08), rough=0.6),
        wood=K.mat("place_wood", K.tex("floor.jpg"), K.tex("floor_n.jpg"), rough=0.4),
        bulb=K.mat("place_bulb", color=(1.0, 0.85, 0.6), emit=(1.0, 0.72, 0.42), emit_strength=10.0, baked=False),
        glow=K.mat("place_glow", color=(1.0, 0.9, 0.75), emit=(1.0, 0.78, 0.52), emit_strength=2.5, baked=False),
    )
    # the balcony slab (its edge shows), the facade with the French doors, the soffit of the balcony above
    EL.floor_rect("balcony_floor", -BX, BX, BY0, BY1, 0.0, M["pavers"], uv=0.9)
    K.box("balcony_edge", (2 * BX + 0.1, 0.12, 0.18), (0.0, BY1 + 0.06, -0.09), M["render"], bevel=0.01)
    EL.floor_rect("balcony_soffit", -BX, BX, BY0, BY1 + 0.12, H + 0.25, M["soffit"], up=False)
    EL.wall_span("facade", "-Y", BY0, -BX - 0.6, BX + 0.6, -0.2, H + 0.4, M["render"], holes=[(DOOR[0], DOOR[1], 0.0, DOOR[2])])
    F = EL.WallFrame("-Y", BY0)
    for k, (a0, a1) in enumerate(((DOOR[0] - 0.06, DOOR[0]), (DOOR[1], DOOR[1] + 0.06))):
        F.box(f"doorframe_{k}", a0, a1, -0.155, 0.02, 0.0, DOOR[2] + 0.06, M["trim"])    # proud of both wall faces
    F.box("doorframe_head", DOOR[0] - 0.06, DOOR[1] + 0.06, -0.155, 0.02, DOOR[2], DOOR[2] + 0.06, M["trim"])
    for k, (hx, sgn) in enumerate(((DOOR[0], 1), (DOOR[1], -1))):          # the two leaves swung open onto the balcony
        x = hx - sgn * 0.03
        for nm, y0, y1, z0, z1 in (("stile_a", 0.0, 0.07, 0.0, DOOR[2] - 0.02), ("stile_b", 0.89, 0.96, 0.0, DOOR[2] - 0.02),
                                   ("rail_b", 0.0, 0.96, 0.0, 0.18), ("rail_m", 0.0, 0.96, 1.0, 1.05), ("rail_t", 0.0, 0.96, DOOR[2] - 0.09, DOOR[2] - 0.02)):
            K.box(f"french_{k}_{nm}", (0.05, y1 - y0, z1 - z0), (x, BY0 + (y0 + y1) / 2, (z0 + z1) / 2), M["trim"], bevel=0.004)
        K.box(f"french_glass_{k}", (0.012, 0.82, DOOR[2] - 0.29), (x, BY0 + 0.48, (0.18 + DOOR[2] - 0.09) / 2), M["glass"])
    # railings: front and sides (balusters + a flat iron handrail), a planter box along the front
    EL.balustrade("rail_front", (-BX + 0.03, BY1 - 0.03), (BX - 0.03, BY1 - 0.03), M, h=1.05, spacing=0.11)
    EL.balustrade("rail_left", (-BX + 0.03, BY0 + 0.05), (-BX + 0.03, BY1 - 0.03), M, h=1.05, spacing=0.11)
    EL.balustrade("rail_right", (BX - 0.03, BY0 + 0.05), (BX - 0.03, BY1 - 0.03), M, h=1.05, spacing=0.11)
    for k, x in enumerate((-1.3, 1.25)):
        EL.plant(f"plant_{k}", x, BY0 + 0.3, M, tall=0.9, blades=11, broad=k == 1)
    # a bistro table and two folding chairs at the right end, out of the standing marks
    tbl = EL.table("bistro", 1.35, -0.05, M, w=0.6, d=0.6, h=0.72, top="wood", legs="iron")   # clear of the open leaf
    EL.chair("bistro_chair_a", 1.35, -0.62, (0, 1), M, seat="wood", frame="iron")
    # string lights: warm bulbs drooping from the facade to the soffit corners
    bulbs = []
    for k in range(13):
        t = k / 12
        x = -BX + 0.1 + t * (2 * BX - 0.2)
        y = BY1 - 0.1
        z = H + 0.05 - 0.28 * math.sin(math.pi * t)
        K.cylinder(f"string_bulb_{k}", 0.025, 0.025, 0.05, (x, y, z - 0.05), M["bulb"], seg=12)
        bulbs.append(Vector((x, y, z - 0.03)))
    # the flat behind: floor, walls, a sofa back and a lamp (the reverse view)
    EL.floor_rect("flat_floor", IR[0], IR[1], IR[2], IR[3], 0.0, M["floor"], uv=0.7)
    EL.floor_rect("flat_ceiling", IR[0], IR[1], IR[2], IR[3], H, M["ceiling"], up=False)
    EL.wall_span("flat_back", "-Y", IR[2], IR[0], IR[1], 0.0, H, M["wall"])
    EL.wall_span("flat_left", "-X", IR[0], IR[2], IR[3], 0.0, H, M["wall"])
    EL.wall_span("flat_right", "+X", IR[1], IR[2], IR[3], 0.0, H, M["wall"])
    EL.wall_span("flat_front", "+Y", IR[3], IR[0], IR[1], 0.0, H, M["wall"], holes=[(DOOR[0], DOOR[1], 0.0, DOOR[2])])
    K.box("flat_sofa", (2.0, 0.9, 0.45), (0.0, IR[2] + 0.6, 0.225), M["sofa"], bevel=0.05, segs=3)
    K.box("flat_sofa_back", (2.0, 0.25, 0.5), (0.0, IR[2] + 0.2, 0.7), M["sofa"], bevel=0.05, segs=3)
    lampI = Vector((-1.9, IR[2] + 0.5, 1.45))
    K.cylinder("flat_lamp_pole", 0.012, 0.012, 1.4, (lampI.x, lampI.y, 0.0), M["iron"], seg=10)
    K.cylinder("flat_lamp_shade", 0.2, 0.15, 0.26, (lampI.x, lampI.y, 1.35), M["glow"], seg=32, cap=False)
    lantern = Vector((DOOR[1] + 0.35, BY0 + 0.1, 2.05))
    K.box("lantern_plate", (0.14, 0.02, 0.2), (lantern.x, BY0 + 0.01, lantern.z), M["iron"])
    K.cylinder("lantern_glass", 0.07, 0.07, 0.2, (lantern.x, lantern.y, lantern.z - 0.1), M["glow"], seg=20)
    city.build_city((0.0, BY1 + 0.15, 0.0), (0, 1), drop=12.0, width=80.0)
    # light: the lantern by the doors (the key), the string lights, the flat's lamp through the doors, the city
    key = K.light("SPOT", "lamp_key", tuple(lantern + Vector((0, 0.1, 0.05))), (1.0, 0.74, 0.48), 0.0, 0.1, spot=math.radians(120))
    K.aim(key, (0.0, -0.3, 0.9))
    K.light("POINT", "lamp_0", tuple(lantern + Vector((0, 0.12, -0.05))), (1.0, 0.72, 0.45), 20.0, 0.06)
    K.light("POINT", "lamp_1", tuple(bulbs[4]), (1.0, 0.72, 0.42), 9.0, 0.05)
    K.light("POINT", "lamp_2", tuple(bulbs[9]), (1.0, 0.72, 0.42), 9.0, 0.05)
    K.light("POINT", "lamp_3", tuple(lampI - Vector((0, 0, 0.1))), (1.0, 0.7, 0.42), 40.0, 0.1)
    K.light("AREA", "flat_wash", (0.0, IR[2] + 1.6, H - 0.05), (1.0, 0.8, 0.6), 50.0, size=1.2)
    K.light("AREA", "city_glow", (0.0, BY1 + 6.0, 1.0), (0.55, 0.62, 0.95), 600.0, rot=(math.radians(-90), 0, 0), size=12.0)
    K.light("SUN", "moon", (0, 8, 8), (0.55, 0.62, 0.95), 0.25, rot=(math.radians(50), 0, math.radians(180)))
    meta = common_meta("apartment", "evening", lantern, Vector((0.0, BY1 + 2.0, 1.4)), (-1.55, -4.3, 0.15), (1.55, 0.2, 2.45), (-1.1, 1.1, -1.2, 0.3))
    meta["anchors"] = {
        "rail": anchor(0.0, BY1 - 0.35, 0.0, (0, 1)), "rail_l": anchor(-0.35, BY1 - 0.35, 0.0, (0, 1)),
        "rail_r": anchor(0.35, BY1 - 0.35, 0.0, (0, 1)), "doorway": anchor(0.0, BY0 - 0.2, 0.0, (0, 1)),
        "bistro_table": anchor(tbl.x, tbl.y, tbl.z, (0, -1)), "railing_top": anchor(0.0, BY1 - 0.03, 1.07, (0, 1)),
    }
    engine = {"lamp_key": 120.0, "lamp_0": 0.4, "lamp_1": 0.12, "lamp_2": 0.12, "lamp_3": 0.4, "moon": 0.25,
              "flat_wash": None, "city_glow": None}
    previews = [("stand", (0.25, -3.2, 1.55), (0.0, 1.5, 1.2)), ("side", (-1.4, -1.1, 1.6), (1.2, 1.0, 0.9)),
                ("city", (0.0, -0.8, 1.6), (0.0, 20.0, -2.0))]
    return meta, engine, previews
