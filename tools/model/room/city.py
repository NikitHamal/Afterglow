# Story places — the city pack (Blender 4.2): a procedural night city to fill windows and balconies. Everything is
# emissive and stays out of the lightmap; it lights the room in the bake through the glass like a real city would.
# The engine clips at 40 m, so the city is a "bigature": the street and the block across it at full scale 12-20 m
# out, the towers behind them built 1:5 from 20 to 36 m (they read as 100-180 m away), a skyline card at ~37 m.
#   build_city(view, facing, drop)  view = a point on the window / balcony floor (Blender), facing = (dx, dy) out
#   into the city, drop = how far the street lies below that floor (m)
import math, random
from mathutils import Vector
import kit as K
import textures as TX

_M = {}


def _mats():
    if _M: return _M
    for k in range(3):
        TX.facade(K.tex(f"facade{k}.jpg"), seed=11 + k, lit=(0.32, 0.45, 0.55)[k], wall=((0.03, 0.03, 0.04), (0.05, 0.04, 0.035), (0.025, 0.03, 0.045))[k])
        _M[f"facade{k}"] = K.mat(f"city_facade{k}", color=(0, 0, 0), emit_tex=K.tex(f"facade{k}.jpg"), emit_strength=1.0, baked=False)
    TX.city_night(K.tex("skyline.jpg"), seed=9)
    _M["sky"] = K.mat("city_skyline", color=(0, 0, 0), emit_tex=K.tex("skyline.jpg"), emit_strength=1.0, baked=False)
    _M["roof"] = K.mat("city_roof", color=(0, 0, 0), emit=(0.012, 0.012, 0.018), emit_strength=1.0, baked=False)
    _M["street"] = K.mat("city_street", color=(0, 0, 0), emit=(0.035, 0.03, 0.028), emit_strength=1.0, baked=False)
    _M["lamp"] = K.mat("city_lamp", color=(0, 0, 0), emit=(1.0, 0.72, 0.4), emit_strength=6.0, baked=False)
    _M["head"] = K.mat("city_headlight", color=(0, 0, 0), emit=(1.0, 0.95, 0.85), emit_strength=5.0, baked=False)
    _M["tail"] = K.mat("city_taillight", color=(0, 0, 0), emit=(1.0, 0.08, 0.04), emit_strength=4.0, baked=False)
    _M["car"] = K.mat("city_car", color=(0, 0, 0), emit=(0.02, 0.02, 0.025), emit_strength=1.0, baked=False)
    return _M


def _tower(name, c, along, out, w, d, h, z0, mat, M, scale=1.0):
    """a block of footprint w (along the street) x d (away), height h, standing at street level z0"""
    p = c
    yaw = math.atan2(out.y, out.x) - math.pi / 2
    K.box(name, (w, d, h), (p.x, p.y, z0 + h / 2), mat, uv_scale=scale / 24.8, rot_z=yaw)
    K.box(f"{name}_roof", (w * 1.01, d * 1.01, 0.1 / scale), (p.x, p.y, z0 + h + 0.05 / scale), M["roof"], rot_z=yaw)


def build_city(view, facing, drop=12.0, width=70.0, seed=3):
    M = _mats()
    rnd = random.Random(seed)
    o = Vector(view)
    out = Vector((facing[0], facing[1], 0)).normalized()
    along = Vector((-out.y, out.x, 0))
    street = o.z - drop
    # the street: asphalt, lamps on both kerbs, a few cars with their lights
    K.box("city_street", (width, 9.0, 0.1), tuple(o + out * 9.5 + Vector((0, 0, -drop - 0.05))), M["street"], rot_z=math.atan2(out.y, out.x) - math.pi / 2)
    for k in range(-5, 6):
        for side in (5.5, 13.5):
            q = o + out * side + along * (k * 7.0 + (1.8 if side > 6 else 0))
            K.box(f"city_lamppost_{k}_{int(side)}", (0.12, 0.12, 5.0), (q.x, q.y, street + 2.5), M["car"])
            K.box(f"city_lamphead_{k}_{int(side)}", (0.5, 0.3, 0.12), (q.x, q.y, street + 5.0), M["lamp"])
    for k in range(7):
        q = o + out * (8.0 + 3.0 * (k % 2)) + along * rnd.uniform(-width / 2.5, width / 2.5)
        dirn = 1 if k % 2 else -1
        car = K.box(f"city_car_{k}", (4.2, 1.8, 1.4), (q.x, q.y, street + 0.7), M["car"], rot_z=math.atan2(along.y, along.x))
        for s_ in (-1, 1):
            front = q + along * (2.12 * dirn) + out * (0.62 * s_)
            back = q - along * (2.12 * dirn) + out * (0.62 * s_)
            K.box(f"city_car_{k}_head{s_}", (0.06, 0.3, 0.14), (front.x, front.y, street + 0.75), M["head"], rot_z=math.atan2(along.y, along.x))
            K.box(f"city_car_{k}_tail{s_}", (0.06, 0.3, 0.12), (back.x, back.y, street + 0.8), M["tail"], rot_z=math.atan2(along.y, along.x))
    # the block across the street: full scale, 4-7 storeys, 14-20 m out
    a = -width / 2
    k = 0
    while a < width / 2:
        w = rnd.uniform(9.0, 16.0); d = rnd.uniform(8.0, 12.0); h = rnd.uniform(4, 7) * 3.1
        c = o + out * (14.5 + d / 2) + along * (a + w / 2)
        _tower(f"city_block_{k}", c, along, out, w - 0.3, d, h, street, M[f"facade{k % 3}"], M)
        a += w; k += 1
    # the towers behind: 1:5 bigature, 20-36 m out (100-180 m at full scale), taller toward the middle
    s = 0.2
    for row, dist in enumerate((22.0, 26.5, 31.0)):
        a = -width / 2 - 10
        while a < width / 2 + 10:
            w = rnd.uniform(14, 34) * s; d = rnd.uniform(14, 30) * s
            centre = 1.0 - min(1.0, abs(a) / (width * 0.6))
            h = rnd.uniform(20, 70 + 110 * centre) * s * (1.0 + 0.35 * row)
            c = o + out * (dist + d / 2) + along * (a + w / 2)
            _tower(f"city_tower_{row}_{k}", c, along, out, w * 0.92, d, h, o.z - drop * s, M[f"facade{(k + row) % 3}"], M, scale=s)   # scaled about the viewer
            a += w * rnd.uniform(1.0, 1.6); k += 1
    # the far skyline and sky: a card just inside the far clip
    c = o + out * 37.0
    sky = K.grid("city_sky", 120.0, 46.0, 1, 1, M["sky"], smooth=False, uv_fit=True)
    sky.rotation_euler = (math.pi / 2, 0, math.atan2(out.y, out.x) + math.pi / 2)
    sky.location = (c.x, c.y, street + 18.0)
