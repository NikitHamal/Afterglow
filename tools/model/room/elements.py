# Story places — elements pack (Blender 4.2): doors that open, stairs, rails, lights and the furniture / decor the
# story places share. Blender coordinates (Z up; engine x = x, y = z, z = -y); materials come from the recipe.
#
# Doors (the contract with the engine): the leaf is ONE node named door_<id>, identity rotation, its origin on the
# hinge line at floor level, so a rotation about engine +Y swings it; openDeg (signed) is the swing into the
# apartment. door() returns the leaf object and the JSON entry for the place's "doors" list.
import bpy, math
from mathutils import Vector, Matrix
import kit as K

# walls: the hall-side normal n (pointing into the room the door is seen from) and the axis the wall runs along
WALL = {"-X": ((1, 0), 1), "+X": ((-1, 0), 1), "-Y": ((0, 1), 0), "+Y": ((0, -1), 0)}


def E(p):
    return [round(p[0], 3), round(p[2], 3), round(-p[1], 3)]


def yaw_of(dx, dy):
    """engine yaw (degrees about +Y, 0 = facing engine +Z) of a facing direction given in Blender x / y"""
    return round(math.degrees(math.atan2(dx, -dy)), 1)


def anchor(x, y, z, face):
    """an anchor for the place JSON: Blender point + the Blender (x, y) direction it faces"""
    return {"pos": E((x, y, z)), "yaw": yaw_of(*face)}


def join(name, objs, origin=None):
    """one object (one glTF node) from several; origin (Blender) becomes its pivot without moving the geometry"""
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs: o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.join()
    ob = bpy.context.view_layer.objects.active
    ob.name = name; ob.data.name = name
    if origin is not None:
        ob.data.transform(Matrix.Translation(-Vector(origin)))
        ob.location = Vector(origin)
        bpy.context.view_layer.update()
    return ob


class WallFrame:
    """maps (a along the wall, d off the wall into the hall, z up) to world, for an axis-aligned wall"""
    def __init__(self, side, fixed):
        (self.nx, self.ny), self.axis = WALL[side]
        self.fixed = fixed

    def p(self, a, d, z):
        return Vector((self.fixed + self.nx * d, a, z)) if self.axis == 1 else Vector((a, self.fixed + self.ny * d, z))

    def box(self, name, a0, a1, d0, d1, z0, z1, mat, bevel=0.0, uv=1.0):
        p0, p1 = self.p(a0, d0, z0), self.p(a1, d1, z1)
        lo = Vector((min(p0.x, p1.x), min(p0.y, p1.y), min(p0.z, p1.z))); hi = Vector((max(p0.x, p1.x), max(p0.y, p1.y), max(p0.z, p1.z)))
        return K.box(name, tuple(hi - lo), tuple((lo + hi) / 2), mat, bevel=bevel, uv_scale=uv)



def door(did, side, fixed, c, M, width=0.9, height=2.05, wall_t=0.12, hinge=-1, number=None):
    """a panelled apartment door in wall `side` (plane at `fixed`, hall on the normal side), centred at c along the
    wall; hinge -1/+1 = the hinge on the low / high end along the wall. Builds casing, jambs, threshold, the leaf
    (lever handles, peephole, number plaque) and returns (leaf, doors-entry, hole for K.wall)."""
    F = WallFrame(side, fixed)
    a0, a1 = c - width / 2, c + width / 2
    # casing on the hall face, jambs through the wall, threshold
    F.box(f"door_{did}_casing_l", a0 - 0.09, a0, 0.0, 0.02, 0.0, height + 0.09, M["trim"], bevel=0.004)
    F.box(f"door_{did}_casing_r", a1, a1 + 0.09, 0.0, 0.02, 0.0, height + 0.09, M["trim"], bevel=0.004)
    F.box(f"door_{did}_casing_t", a0 - 0.09, a1 + 0.09, 0.0, 0.025, height, height + 0.09, M["trim"], bevel=0.004)
    F.box(f"door_{did}_jamb_l", a0 - 0.012, a0, -wall_t, 0.0, 0.0, height, M["trim"])
    F.box(f"door_{did}_jamb_r", a1, a1 + 0.012, -wall_t, 0.0, 0.0, height, M["trim"])
    F.box(f"door_{did}_jamb_t", a0, a1, -wall_t, 0.0, height, height + 0.012, M["trim"])
    F.box(f"door_{did}_threshold", a0, a1, -wall_t, 0.012, 0.0, 0.012, M["brass"], bevel=0.003)
    # the leaf, set back 3 cm from the hall face; parts on both faces
    t, dc = 0.045, -0.03 - 0.0225
    parts = [F.box(f"door_{did}_slab", a0 + 0.004, a1 - 0.004, dc - t / 2, dc + t / 2, 0.006, height - 0.004, M["door"], bevel=0.003)]
    for k, (z0, z1) in enumerate(((0.16, 0.92), (1.08, height - 0.16))):
        for s in (-1, 1):
            d0 = dc + s * (t / 2)
            parts.append(F.box(f"door_{did}_panel_{k}{s}", a0 + 0.13, a1 - 0.13, d0, d0 + s * 0.008, z0, z1, M["door"], bevel=0.006))
    latch = a1 - 0.075 if hinge < 0 else a0 + 0.075
    for s in (-1, 1):
        d0 = dc + s * t / 2
        parts.append(F.box(f"door_{did}_rose{s}", latch - 0.03, latch + 0.03, d0, d0 + s * 0.01, 0.97, 1.05, M["brass"], bevel=0.004))
        lev = (latch - 0.13, latch) if hinge < 0 else (latch, latch + 0.13)
        parts.append(F.box(f"door_{did}_lever{s}", lev[0], lev[1], d0 + s * 0.035, d0 + s * 0.055, 1.0, 1.02, M["brass"], bevel=0.006))
        parts.append(F.box(f"door_{did}_lever_neck{s}", latch - 0.01, latch + 0.01, d0, d0 + s * 0.05, 1.0, 1.02, M["brass"]))
    hall = dc + t / 2
    parts.append(F.box(f"door_{did}_peep", c - 0.012, c + 0.012, hall, hall + 0.012, 1.5, 1.524, M["brass"]))
    if number:
        parts.append(F.box(f"door_{did}_plaque", c - 0.11, c + 0.11, hall, hall + 0.006, 1.6, 1.71, M[f"plaque_{number}"]))
    hinge_a = a0 if hinge < 0 else a1
    pivot = F.p(hinge_a, dc, 0.0)
    leaf = join(f"door_{did}", parts, origin=pivot)
    # the swing: into the apartment (away from the hall), the free end moving off the hinge
    free = F.p(a1 if hinge < 0 else a0, dc, 0.0) - pivot
    into = Vector((-F.nx, -F.ny, 0.0))
    dx_e, dz_e = free.x, -free.y                    # engine x / z of the leaf direction
    ox_e, oz_e = into.x, -into.y
    sign = 1.0 if dz_e * ox_e + (-dx_e) * oz_e > 0 else -1.0
    entry = {"id": did, "node": f"door_{did}", "openDeg": sign * 100.0, "hinge": E(pivot), "width": width,
             "note": "rotate the node about engine +Y by up to openDeg to open it (identity rest rotation)"}
    return leaf, entry, (a0 - 0.012, a1 + 0.012, 0.0, height + 0.012)


def bell(name, side, fixed, a, M, z=1.32):
    F = WallFrame(side, fixed)
    F.box(f"{name}_plate", a - 0.035, a + 0.035, 0.0, 0.008, z - 0.05, z + 0.05, M["brass"], bevel=0.003)
    F.box(f"{name}_button", a - 0.012, a + 0.012, 0.008, 0.016, z - 0.012, z + 0.012, M["button"], bevel=0.004)


def sconce(name, side, fixed, a, z, M):
    """a wall light: back plate, arm, a glowing glass shade; returns the light's position"""
    F = WallFrame(side, fixed)
    F.box(f"{name}_plate", a - 0.05, a + 0.05, 0.0, 0.012, z - 0.09, z + 0.03, M["brass"], bevel=0.004)
    F.box(f"{name}_arm", a - 0.012, a + 0.012, 0.012, 0.11, z - 0.02, z, M["brass"])
    p = F.p(a, 0.13, z + 0.06)
    K.cylinder(f"{name}_shade", 0.065, 0.085, 0.16, tuple(p - Vector((0, 0, 0.08))), M["glow"], seg=28)
    return p


def doormat(name, side, fixed, a, M, w=0.8, depth=0.5):
    F = WallFrame(side, fixed)
    return F.box(name, a - w / 2, a + w / 2, 0.06, 0.06 + depth, 0.0, 0.014, M["mat"], bevel=0.004, uv=1.6)


def label_uv(ob, center, rot_z=0.0):
    """box faces mapped 0..1 each (a label centred on every side), in the box's own (unrotated) frame"""
    me = ob.data; uv = me.uv_layers.get("UVMap") or me.uv_layers.new(name="UVMap")
    c, s = math.cos(-rot_z), math.sin(-rot_z)
    pts = [Vector((c * (v.co.x - center[0]) - s * (v.co.y - center[1]), s * (v.co.x - center[0]) + c * (v.co.y - center[1]), v.co.z - center[2])) for v in me.vertices]
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts))); hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    span = hi - lo
    for poly in me.polygons:
        n = poly.normal; nl = Vector((c * n.x - s * n.y, s * n.x + c * n.y, n.z))
        ax = max(range(3), key=lambda i: abs(nl[i]))
        for li in poly.loop_indices:
            q = pts[me.loops[li].vertex_index] - lo
            u, v = (q.y / span.y, q.z / span.z) if ax == 0 else ((q.x / span.x, q.z / span.z) if ax == 1 else (q.x / span.x, q.y / span.y))
            uv.data[li].uv = (u, v)


def boxes(name, base, M, specs):
    """moving boxes: specs = [(dx, dy, z, sx, sy, sz, rot_deg, label)] from base (Blender x, y)"""
    out = []
    for k, (dx, dy, z, sx, sy, sz, rot, label) in enumerate(specs):
        ctr = (base[0] + dx, base[1] + dy, z + sz / 2)
        ob = K.box(f"{name}_{k}", (sx, sy, sz), ctr, M[f"box_{label}"], bevel=0.008, rot_z=math.radians(rot))
        label_uv(ob, ctr, math.radians(rot))
        out.append(ob)
    return out


def balustrade(name, p0, p1, M, h=0.95, spacing=0.12, z0=0.0, z1=None):
    """posts and a handrail from p0 to p1 (Blender x, y); the rail follows the floor from z0 to z1 (stairs)"""
    z1 = z0 if z1 is None else z1
    a, b = Vector((p0[0], p0[1], z0)), Vector((p1[0], p1[1], z1))
    L = (Vector((b.x, b.y, 0)) - Vector((a.x, a.y, 0))).length
    n = max(2, int(L / spacing))
    for k in range(n + 1):
        q = a.lerp(b, k / n)
        K.box(f"{name}_post{k}", (0.018, 0.018, h - 0.05), (q.x, q.y, q.z + (h - 0.05) / 2), M["iron"])
    d = b - a
    rail = K.box(f"{name}_rail", (d.length + 0.06, 0.055, 0.04), (0.0, 0.0, 0.0), M["rail"], bevel=0.012)   # built at the origin,
    rail.rotation_euler = (0, -math.atan2(d.z, Vector((d.x, d.y, 0)).length), math.atan2(d.y, d.x))       # then turned and placed
    rail.location = (a + b) / 2 + Vector((0, 0, h)); bpy.context.view_layer.update()
    return rail


def stair_flight(name, x0, x1, y0, steps, rise, tread, M, z0=0.0):
    """a flight going down along +y from (y0, z0): treads + risers between x0 and x1; returns the end (y, z)"""
    y, z = y0, z0
    for k in range(steps):
        K.box(f"{name}_riser{k}", (x1 - x0, 0.02, rise), ((x0 + x1) / 2, y + 0.01, z - rise / 2), M["riser"])
        z -= rise
        K.box(f"{name}_tread{k}", (x1 - x0 + 0.02, tread + 0.025, 0.035), ((x0 + x1) / 2, y + tread / 2, z - 0.0175 + 0.0), M["tread"], bevel=0.006, uv_scale=1.4)
        y += tread
    return y, z


def mailboxes(name, side, fixed, a, M, cols=4, rows=2, z=1.05):
    F = WallFrame(side, fixed)
    w, h = 0.26, 0.2
    for r in range(rows):
        for c in range(cols):
            a0 = a + (c - cols / 2) * (w + 0.01)
            z0 = z + r * (h + 0.01)
            F.box(f"{name}_{r}{c}", a0, a0 + w, 0.0, 0.16, z0, z0 + h, M["metal"], bevel=0.004)
            F.box(f"{name}_slot{r}{c}", a0 + 0.04, a0 + w - 0.04, 0.16, 0.165, z0 + h - 0.06, z0 + h - 0.045, M["iron"])


def plant(name, x, y, M, tall=1.1, blades=10, broad=False):
    K.cylinder(f"{name}_pot", 0.17, 0.13, 0.36, (x, y, 0.0), M["pot"], seg=28)
    for k in range(blades):
        a = k * 2.4
        lean = (0.5 if broad else 0.14) + 0.06 * (k % 3)
        K.cylinder(f"{name}_leaf{k}", 0.045 if broad else 0.012, 0.0, tall * (0.75 + 0.1 * (k % 3)), (x, y, 0.34), M["leaf"], seg=6).rotation_euler = (lean * math.cos(a), lean * math.sin(a), a)


def wall_span(name, side, fixed, a0, a1, z0, z1, mat, holes=(), uv=0.9):
    """an axis-aligned wall piece a0..a1 along the wall, z0..z1 high (any level: stair shafts), facing the hall side,
    with rectangular openings holes=[(a, b, zh0, zh1)]"""
    F = WallFrame(side, fixed)
    rects, u = [], a0
    for a, b, h0, h1 in sorted(holes):
        if a > u: rects.append((u, a, z0, z1))
        if h0 > z0: rects.append((a, b, z0, h0))
        if h1 < z1: rects.append((a, b, h1, z1))
        u = b
    if u < a1: rects.append((u, a1, z0, z1))
    obs = []
    for k, (a, b, h0, h1) in enumerate(rects):
        cs = [F.p(a, 0, h0), F.p(b, 0, h0), F.p(b, 0, h1), F.p(a, 0, h1)]
        obs.append(K.quad(f"{name}_{k}", [tuple(c) for c in cs], mat, (F.nx, F.ny, 0), uv))
    return obs


def floor_rect(name, x0, x1, y0, y1, z, mat, up=True, uv=0.8):
    return K.quad(name, [(x0, y0, z), (x1, y0, z), (x1, y1, z), (x0, y1, z)], mat, (0, 0, 1 if up else -1), uv)


def skirting(name, side, fixed, a0, a1, M, h=0.12, z0=0.0):
    F = WallFrame(side, fixed)
    return F.box(name, a0, a1, 0.0, 0.016, z0, z0 + h, M["trim"], bevel=0.003)


def table(name, x, y, M, w=1.4, d=0.85, h=0.75, top="wood", legs="iron", rot=0.0):
    """a dining / side table; returns the top's centre (Blender) for a "table" anchor"""
    K.box(f"{name}_top", (w, d, 0.04), (x, y, h - 0.02), M[top], bevel=0.008, rot_z=rot, uv_scale=1.2)
    c, s = math.cos(rot), math.sin(rot)
    for sx in (-1, 1):
        for sy in (-1, 1):
            lx, ly = sx * (w / 2 - 0.08), sy * (d / 2 - 0.08)
            K.box(f"{name}_leg{sx}{sy}", (0.04, 0.04, h - 0.04), (x + lx * c - ly * s, y + lx * s + ly * c, (h - 0.04) / 2), M[legs])
    return Vector((x, y, h))


def chair(name, x, y, face, M, seat="wood", frame="iron"):
    """a dining chair at (x, y) facing `face` (Blender dx, dy)"""
    rot = math.atan2(face[1], face[0])
    c, s = math.cos(rot), math.sin(rot)
    K.box(f"{name}_seat", (0.44, 0.44, 0.04), (x, y, 0.46), M[seat], bevel=0.01, rot_z=rot)
    bx, by = x - 0.2 * c, y - 0.2 * s
    K.box(f"{name}_back", (0.03, 0.42, 0.42), (bx, by, 0.7), M[seat], bevel=0.008, rot_z=rot)
    for sx in (-1, 1):
        for sy in (-1, 1):
            lx, ly = sx * 0.18, sy * 0.18
            K.box(f"{name}_leg{sx}{sy}", (0.03, 0.03, 0.44), (x + lx * c - ly * s, y + lx * s + ly * c, 0.22), M[frame])


def counter(name, x0, x1, y_wall, depth, M, h=0.9, wall_side=1, upper=True):
    """a kitchen run along a +-Y wall from x0 to x1: base cabinets, worktop, upper cabinets; returns the worktop centre"""
    yc = y_wall - wall_side * depth / 2
    K.box(f"{name}_base", (x1 - x0, depth - 0.02, h - 0.04), ((x0 + x1) / 2, yc, (h - 0.04) / 2), M["cabinet"], bevel=0.004)
    K.box(f"{name}_top", (x1 - x0 + 0.02, depth + 0.02, 0.04), ((x0 + x1) / 2, yc - wall_side * 0.01, h - 0.02), M["worktop"], bevel=0.004, uv_scale=1.4)
    n = max(1, int((x1 - x0) / 0.6))
    for k in range(n):
        cx = x0 + (k + 0.5) * (x1 - x0) / n
        K.box(f"{name}_door{k}", ((x1 - x0) / n - 0.02, 0.01, h - 0.16), (cx, y_wall - wall_side * (depth + 0.0), (h - 0.08) / 2), M["cabinet"], bevel=0.003)
        K.box(f"{name}_pull{k}", (0.12, 0.02, 0.012), (cx, y_wall - wall_side * (depth + 0.012), h - 0.16), M["brass"])
    if upper:
        K.box(f"{name}_upper", (x1 - x0, 0.34, 0.7), ((x0 + x1) / 2, y_wall - wall_side * 0.17, 1.85), M["cabinet"], bevel=0.004)
    return Vector(((x0 + x1) / 2, yc, h))


def piano(name, x, y, face, M):
    """an upright piano against a wall, keys facing `face` (Blender dx, dy)"""
    rot = math.atan2(face[1], face[0])
    c, s = math.cos(rot), math.sin(rot)
    def at(fwd, z): return (x + fwd * c, y + fwd * s, z)
    K.box(f"{name}_body", (0.58, 1.48, 1.22), at(0.0, 0.61), M["lacquer"], bevel=0.01, rot_z=rot)
    K.box(f"{name}_keybed", (0.3, 1.4, 0.06), at(0.4, 0.72), M["lacquer"], bevel=0.006, rot_z=rot)
    K.box(f"{name}_keys", (0.16, 1.32, 0.02), at(0.47, 0.76), M["ivory"], rot_z=rot)
    for k in range(22):
        off = -0.62 + k * 0.059
        if k % 7 in (2, 6): continue
        K.box(f"{name}_sharp{k}", (0.09, 0.022, 0.016), (x + 0.44 * c - off * s, y + 0.44 * s + off * c, 0.78), M["lacquer"], rot_z=rot)
    K.box(f"{name}_bench", (0.36, 0.8, 0.48), at(0.9, 0.24), M["lacquer"], bevel=0.01, rot_z=rot)


def frames_on_wall(name, side, fixed, a0, a1, z0, z1, M, mats, cols=4, rows=2):
    """paper drawings pinned in a loose grid on a wall"""
    from elements import WallFrame
    F = WallFrame(side, fixed)
    import random as _r
    rnd = _r.Random(7)
    k = 0
    for r_ in range(rows):
        for c_ in range(cols):
            a = a0 + (c_ + 0.5) * (a1 - a0) / cols + rnd.uniform(-0.05, 0.05)
            z = z0 + (r_ + 0.5) * (z1 - z0) / rows + rnd.uniform(-0.04, 0.04)
            w, h = rnd.uniform(0.34, 0.48), rnd.uniform(0.26, 0.36)
            ob = F.box(f"{name}_{k}", a - w / 2, a + w / 2, 0.004, 0.008, z - h / 2, z + h / 2, mats[k % len(mats)])
            uv = ob.data.uv_layers[0]
            for li, l in enumerate(ob.data.loops):
                v = ob.data.vertices[l.vertex_index].co
                along = v.y if F.axis == 1 else v.x
                uv.data[li].uv = ((along - (a - w / 2)) / w * (1 if (F.nx if F.axis == 1 else -F.ny) > 0 else -1), (v.z - (z - h / 2)) / h)
            k += 1


def panorama(name, centre, radius, height, eye, mat, seg=96):
    """an outdoor place's backdrop: an open cylinder around it (faces inward) carrying a 360-degree picture whose
    horizon (v = 0.5) sits at eye height, u = 0.5 straight out along +Y and u turning right (toward +X), closed by a
    sky cap and a floor disc that sample the picture's top and bottom rows. Keep radius + the farthest camera's offset
    from `centre` inside the engine's far clip (40 m). Seen by the camera only: it adds no light to the bake."""
    import bmesh
    bm = bmesh.new(); uv = bm.loops.layers.uv.new("UVMap")
    cx, cy = centre
    z0, z1 = eye - height / 2, eye + height / 2
    ring = [(cx + radius * math.sin(2 * math.pi * (k / seg - 0.5)), cy + radius * math.cos(2 * math.pi * (k / seg - 0.5))) for k in range(seg + 1)]
    lo = [bm.verts.new((x, y, z0)) for x, y in ring]
    hi = [bm.verts.new((x, y, z1)) for x, y in ring]
    ctop, cbot = bm.verts.new((cx, cy, z1)), bm.verts.new((cx, cy, z0))
    for k in range(seg):                                   # the angle runs clockwise seen from above: these face inward
        f = bm.faces.new((lo[k], lo[k + 1], hi[k + 1], hi[k]))
        for l, t in zip(f.loops, ((k / seg, 0.0), ((k + 1) / seg, 0.0), ((k + 1) / seg, 1.0), (k / seg, 1.0))): l[uv].uv = t
        for vs, t in (((ctop, hi[k], hi[k + 1]), (0.5, 0.999)), ((cbot, lo[k + 1], lo[k]), (0.5, 0.001))):
            for l in bm.faces.new(vs).loops: l[uv].uv = t
    ob = K.from_bm(name, bm, mat)
    for vis in ("visible_diffuse", "visible_glossy", "visible_transmission", "visible_volume_scatter", "visible_shadow"):
        setattr(ob, vis, False)
    return ob
