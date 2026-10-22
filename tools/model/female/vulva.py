# Anatomical vulva + anus for the MPFB female base mesh (exec'd from build5.py; also runnable standalone).
#
# MakeHuman's female crotch is a coarse (~19 mm edges), featureless surface. This module:
#   1. subdivides the crotch region three times (smooth), so features get ~2.5 mm resolution;
#   2. sculpts analytic profiles laid out along the body's midline arc (s) and lateral offset (x):
#      labia majora pads, the vulvar cleft, ruffled labia minora, clitoral hood + glans, vestibule,
#      urethral dimple, perineal raphe, and a puckered anus (radial pleats);
#   3. cuts real openings — a ~34 mm vaginal canal and a short anal canal — with stepped insets
#      pushed inward along the anatomical axis, closed at the bottom, UV-mapped into free texture space;
#   4. stores per-vertex anatomy masks (float attributes) for the skin painter:
#      v_muc (mucosa), v_min (minora pigment), v_maj (outer pigment), v_clit, v_cav (canal depth 0..1),
#      v_anus (anal pigment), v_pub (landing-strip area).
# Shape keys are removed before the edit and re-added afterwards from per-vertex deltas, so expression
# and body-shape morphs keep working and never disturb this region.
import bpy, bmesh, math
import numpy as np
from mathutils import Vector

def build_vulva(body, cov=None, log=print):
    me = body.data
    # ---------------------------------------------------------------- shape keys -> deltas, then clear
    keys = []
    if me.shape_keys:
        kb0 = me.shape_keys.key_blocks[0]
        B = np.empty(len(me.vertices) * 3, np.float32); kb0.data.foreach_get("co", B); B = B.reshape(-1, 3)
        for kb in me.shape_keys.key_blocks[1:]:
            a = np.empty(len(kb.data) * 3, np.float32); kb.data.foreach_get("co", a)
            keys.append((kb.name, a.reshape(-1, 3) - B, kb.value))
        body.shape_key_clear()
    n0 = len(me.vertices)

    gi_body = body.vertex_groups["body"].index
    bm = bmesh.new(); bm.from_mesh(me)
    dl = bm.verts.layers.deform.active
    uvl = bm.loops.layers.uv.active
    P0 = np.array([tuple(v.co) for v in bm.verts])

    # ---------------------------------------------------------------- midline arc (x = 0 profile)
    mid = [v for v in bm.verts if abs(v.co.x) < 0.0008 and v[dl].get(gi_body, 0) > 0.5 and 0.70 < v.co.z < 1.05]
    c = Vector((0, -0.02, 0.88))
    mid.sort(key=lambda v: math.atan2(v.co.y - c.y, -(v.co.z - c.z)))       # cut of the angle lies above the pelvis
    pts = np.array([(v.co.y, v.co.z) for v in mid])
    # keep the crotch run: from the lower mons (front, y<0) under the body to the gluteal cleft (back)
    pts = pts[(pts[:, 1] < 1.0)]
    # Catmull-Rom resample at 0.5 mm
    def cr(p0, p1, p2, p3, t):
        return 0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t + (-p0 + 3 * p1 - 3 * p2 + p3) * t ** 3)
    P = np.vstack([pts[0] * 2 - pts[1], pts, pts[-1] * 2 - pts[-2]])
    dense = []
    for i in range(1, len(P) - 2):
        for t in np.linspace(0, 1, 40, endpoint=False):
            dense.append(cr(P[i - 1], P[i], P[i + 1], P[i + 2], t))
    dense = np.array(dense)
    seg = np.linalg.norm(np.diff(dense, axis=0), axis=1); arc = np.concatenate([[0], np.cumsum(seg)])
    tang = np.gradient(dense, axis=0); tang /= np.linalg.norm(tang, axis=1, keepdims=True)
    nrm2 = np.stack([-tang[:, 1], tang[:, 0]], 1)
    inside = np.array([c.y, c.z])
    flip = ((dense - inside) * nrm2).sum(1) < 0
    nrm2[flip] *= -1                                                     # outward from the pelvis
    def arc_at(y, z):
        d = (dense[:, 0] - y) ** 2 + (dense[:, 1] - z) ** 2; i = int(np.argmin(d)); return arc[i], i
    s_A, _ = arc_at(-0.110, 0.866)          # anterior commissure / clitoral hood
    s_P, _ = arc_at(-0.036, 0.8415)         # posterior commissure (fourchette)
    s_N, _ = arc_at(0.004, 0.8445)          # anus
    Lv = s_P - s_A
    log(f"vulva: midline {len(mid)} verts, vulva length {Lv*1000:.0f} mm, perineum {(s_N - s_P)*1000:.0f} mm")

    def local(co, with_dist=False):
        """(x, s, i_arc[, distance to the arc in the y-z plane]) for an array of points"""
        y, z = co[:, 1], co[:, 2]
        d = (dense[None, :, 0] - y[:, None]) ** 2 + (dense[None, :, 1] - z[:, None]) ** 2
        i = np.argmin(d, 1)
        if with_dist: return co[:, 0], arc[i], i, np.sqrt(d[np.arange(len(i)), i])
        return co[:, 0], arc[i], i

    # ---------------------------------------------------------------- 1. subdivide the region (3x, smooth)
    def in_region(co):
        x, s, _ = local(np.atleast_2d(co))
        return (np.abs(x) < 0.050) & (s > s_A - 0.045) & (s < s_N + 0.035)
    def region_faces():
        F = [f for f in bm.faces if all(v[dl].get(gi_body, 0) > 0.5 for v in f.verts)]
        C = np.array([tuple(f.calc_center_median()) for f in F])
        m = in_region(C)
        return [f for f, k in zip(F, m) if k]
    for it in range(3):
        fs = region_faces()
        edges = list({e for f in fs for e in f.edges})
        bmesh.ops.subdivide_edges(bm, edges=edges, cuts=1, use_grid_fill=True, smooth=1.0, quad_corner_type="INNER_VERT")
        log(f"vulva: subdivision pass {it+1}: {len(fs)} faces refined")
    F = [f for f in bm.faces if all(v[dl].get(gi_body, 0) > 0.5 for v in f.verts)]
    C = np.array([tuple(f.calc_center_median()) for f in F]); fx, fs_, _, fd = local(C, True)
    fu = (fs_ - s_A) / Lv
    inner = [f for f, a, b, c_, d_ in zip(F, fx, fs_, fu, fd) if d_ < 0.02 and ((abs(a) < 0.019 and -0.18 < c_ < 1.12) or (a * a + (b - s_N) ** 2) < 0.016 ** 2)]
    bmesh.ops.subdivide_edges(bm, edges=list({e for f in inner for e in f.edges}), cuts=1, use_grid_fill=True, smooth=1.0, quad_corner_type="INNER_VERT")
    log(f"vulva: fine pass on inner vulva + anus: {len(inner)} faces refined")
    bm.verts.ensure_lookup_table(); bm.faces.ensure_lookup_table()
    from mathutils.kdtree import KDTree
    kd = KDTree(len(P0))
    for i, p in enumerate(P0): kd.insert(p, i)
    kd.balance()
    MASK_NAMES = ("v_muc", "v_min", "v_maj", "v_clit", "v_anus", "v_pub", "v_cav")
    oi_l = bm.verts.layers.int.new("oi")
    LAY = {nm: bm.verts.layers.float.new(nm) for nm in MASK_NAMES}   # all layers before any wrapper is held
    oi_l = bm.verts.layers.int["oi"]
    n_tag = 0
    for v in bm.verts:
        co, i, d = kd.find(v.co)
        v[oi_l] = (i + 1) if d < 1e-7 else 0
        n_tag += d < 1e-7
    log(f"vulva: {n_tag} original vertices tagged after subdivision")
    # gentle smoothing of the refined region (fixed rim) to remove the old coarse faceting
    reg = [v for v in bm.verts if v[oi_l] == 0 or in_region(np.array(v.co))[0]]
    regset = set(reg)
    for _ in range(4):
        newco = {}
        for v in reg:
            nb = [e.other_vert(v) for e in v.link_edges]
            if len(nb) < 3 or any(n not in regset for n in nb): continue
            avg = sum((n.co for n in nb), Vector()) / len(nb)
            newco[v] = v.co.lerp(avg, 0.45)
        for v, co in newco.items(): v.co = co
    bm.normal_update()

    # ---------------------------------------------------------------- 2. sculpt + masks
    V = [v for v in bm.verts]
    CO = np.array([tuple(v.co) for v in V]); NO = np.array([tuple(v.normal) for v in V])
    x, s, ia, dist = local(CO, True)
    ax = np.abs(x); u = (s - s_A) / Lv
    n_arc = np.stack([np.zeros(len(ia)), nrm2[ia, 0], nrm2[ia, 1]], 1)
    align = np.clip(((NO * n_arc).sum(1) - 0.3) / 0.4, 0, 1)            # crotch-facing surface only (not the inner thighs)
    def ss(e0, e1, t):
        t = np.clip((t - e0) / (e1 - e0), 0, 1); return t * t * (3 - 2 * t)
    def env(t, a, b, c_, d): return ss(a, b, t) * ss(d, c_, t)
    near = (ax < 0.045) & (s > s_A - 0.06) & (s < s_N + 0.03)
    uc = np.clip(u, 0, 1)
    xm = 0.0105 + 0.0035 * np.sin(np.pi * uc)
    maj = 0.0034 * np.exp(-((ax - xm) / 0.0072) ** 2) * env(u, -0.15, 0.05, 0.92, 1.12)
    cw = 0.0026 + 0.0014 * ss(0.1, 0.6, u)
    cleft = -0.0050 * np.exp(-(x / cw) ** 2) * env(u, -0.02, 0.12, 0.93, 1.06)
    xn = 0.0030 + 0.0014 * np.sin(np.pi * np.clip((u - 0.12) / 0.8, 0, 1))
    side_k = np.where(x > 0, 1.0, 0.86)                                   # natural asymmetry
    ruff = 1 + 0.28 * np.sin(u * 19 + np.sign(x) * 1.3) + 0.12 * np.sin(u * 47 + np.sign(x) * 0.4)
    mino = 0.0044 * side_k * ruff * np.exp(-((ax - xn) / 0.0019) ** 2) * env(u, 0.07, 0.22, 0.86, 0.99)
    vest = -0.0030 * np.exp(-(x / 0.0024) ** 2) * env(u, 0.22, 0.32, 0.88, 0.97)
    hood = 0.0026 * np.exp(-(x / 0.0034) ** 2 - ((u - 0.11) / 0.07) ** 2)
    glans = 0.0013 * np.exp(-(x / 0.0016) ** 2 - ((u - 0.17) / 0.022) ** 2)
    hood -= 0.0009 * np.exp(-(x / 0.0030) ** 2 - ((u - 0.152) / 0.012) ** 2)          # fold between hood and glans
    ureth = -0.0006 * np.exp(-(x / 0.0009) ** 2 - ((u - 0.5) / 0.02) ** 2)
    raphe = 0.0003 * np.exp(-(x / 0.0008) ** 2) * env(s, s_P + 0.003, s_P + 0.008, s_N - 0.008, s_N - 0.004)
    da = s - s_N; ar = np.sqrt(x ** 2 + da ** 2); th = np.arctan2(da, x)
    funnel = -0.0026 * np.exp(-(ar / 0.0075) ** 2)
    pleat = 0.0006 * np.cos(14 * th) * ss(0.0024, 0.0042, ar) * ss(0.011, 0.007, ar)
    near = near & (dist < 0.03)
    disp = (maj + cleft + mino + vest + hood + glans + ureth + raphe + funnel + pleat) * near * align
    for k, v in enumerate(V):
        if disp[k] != 0.0: v.co += v.normal * float(disp[k])
    masks = {
        "v_muc": np.clip(ss(xn + 0.0012, xn - 0.0004, ax) * env(u, 0.12, 0.24, 0.9, 0.99), 0, 1) * near,
        "v_min": np.exp(-((ax - xn) / 0.0017) ** 2) * env(u, 0.07, 0.2, 0.88, 1.0) * near,
        "v_maj": (np.exp(-((ax - xm) / 0.010) ** 2) * env(u, -0.12, 0.05, 0.95, 1.18) + 0.5 * env(s, s_P - 0.002, s_P + 0.004, s_N - 0.004, s_N + 0.01) * np.exp(-(x / 0.008) ** 2)) * near,
        "v_clit": np.exp(-(x / 0.004) ** 2 - ((u - 0.13) / 0.06) ** 2) * near,
        "v_anus": np.exp(-(ar / 0.0085) ** 2) * near,
        "v_pub": ss(0.0105, 0.0065, ax) * env(s, s_A - 0.058, s_A - 0.05, s_A - 0.012, s_A - 0.004),
        "v_cav": np.zeros(len(V)),
    }
    for k, v in enumerate(V):
        for nm, arr in masks.items(): v[LAY[nm]] = float(arr[k])
    bm.normal_update()
    log("vulva: sculpted (majora, cleft, minora, hood, glans, vestibule, raphe, anus pleats)")

    # ---------------------------------------------------------------- 3. openings: stepped insets pushed inward
    def free_rect(px=150):
        """a px x px block of texture space no UV island uses (from a 4K coverage bake)"""
        if cov is None: return (0.02, 0.02, 0.035, 0.035)
        R = cov.shape[0]; occ = cov.astype(np.uint8)
        found = []
        for yy in range(64, R - px - 64, 48):
            for xx in range(64, R - px - 64, 48):
                if not occ[yy - 16:yy + px + 16, xx - 16:xx + px + 16].any():
                    found.append((xx / R, yy / R, px / R, px / R))
                    if len(found) >= 2: return found
        return found
    rects = free_rect()
    def opening(center_s, half_s, half_x, depths, scales, tilt, rect, name):
        _, ic = arc_at(*[float(np.interp(center_s, arc, dense[:, k])) for k in (0, 1)])
        cen = Vector((0.0, dense[ic, 0], dense[ic, 1]))
        n_out = Vector((0.0, nrm2[ic, 0], nrm2[ic, 1])); t_fw = Vector((0.0, tang[ic, 0], tang[ic, 1]))
        d_in = (-n_out * math.cos(tilt) + t_fw * math.sin(tilt)).normalized()
        F = [f for f in bm.faces if all(v[dl].get(gi_body, 0) > 0.5 for v in f.verts)]
        C = np.array([tuple(f.calc_center_median()) for f in F])
        fx, fs_, _, fd = local(C, True)
        sel = [f for f, a, b, dd in zip(F, fx, fs_, fd) if dd < 0.012 and (a / half_x) ** 2 + ((b - center_s) / half_s) ** 2 < 1.0]
        if len(sel) < 3: log(f"WARN opening {name}: only {len(sel)} faces"); return []
        surf = sum((v.co for f in sel for v in f.verts), Vector()) / sum(len(f.verts) for f in sel)
        walls, prev_d, prev_sc = [], 0.0, 1.0
        dmax = depths[-1]
        for k, (dep, sc) in enumerate(zip(depths, scales)):
            res = bmesh.ops.inset_region(bm, faces=sel, thickness=0.0004, depth=0.0, use_even_offset=True)
            walls += res["faces"]
            inner = {v for f in sel for v in f.verts}
            for v in inner:
                rel = v.co - (surf + d_in * prev_d)
                v.co = surf + d_in * dep + rel * (sc / prev_sc)
                v[LAY["v_cav"]] = max(v[LAY["v_cav"]], dep / dmax); v[LAY["v_muc"]] = 1.0
            prev_d, prev_sc = dep, sc
        for f in walls + sel: f.smooth = True
        # UVs: (angle around the canal, depth) into the free rect
        ref = (n_out.cross(d_in)).normalized() if n_out.cross(d_in).length > 1e-6 else Vector((1, 0, 0))
        ref2 = d_in.cross(ref)
        for f in walls + sel:
            uvs = []
            for lp in f.loops:
                rel = lp.vert.co - surf
                a = math.atan2(rel.dot(ref2), rel.dot(ref)); dd = max(0.0, rel.dot(d_in))
                uvs.append([a, dd])
            angs = [q[0] for q in uvs]
            if max(angs) - min(angs) > math.pi:
                for q in uvs:
                    if q[0] < 0: q[0] += 2 * math.pi
            for lp, (a, dd) in zip(f.loops, uvs):
                lp[uvl].uv = (rect[0] + rect[2] * ((a + math.pi) / (3 * math.pi)), rect[1] + rect[3] * min(1.0, dd / dmax))
        log(f"vulva: opening '{name}' cut: {len(sel)} faces, depth {depths[-1]*1000:.0f} mm")
        return True
    if len(rects) >= 2:
        opening(s_A + 0.80 * Lv, 0.0066, 0.0038, [0.003, 0.008, 0.015, 0.024, 0.034], [0.85, 0.74, 0.64, 0.54, 0.40], math.radians(28), rects[0], "vaginal canal")
        opening(s_N, 0.0030, 0.0030, [0.002, 0.006, 0.013], [0.70, 0.50, 0.32], math.radians(35), rects[1], "anal canal")
    else:
        log("WARN vulva: no free UV space found for the canals; openings skipped")
    bm.normal_update()

    # ---------------------------------------------------------------- write back: mesh, masks, shape keys
    bm.verts.index_update()
    oi = np.array([v[oi_l] for v in bm.verts]); is_orig = oi > 0; oidx = oi - 1
    newpos0 = np.array([tuple(v.co) for v in bm.verts])
    bm.to_mesh(me); bm.free(); me.update()
    if "oi" in me.attributes: me.attributes.remove(me.attributes["oi"])
    for p in me.polygons: p.use_smooth = True
    if keys:
        # deltas for new vertices: inverse-distance blend of the 8 nearest original vertices
        lo_, hi_ = newpos0[~is_orig].min(0) - 0.03, newpos0[~is_orig].max(0) + 0.03
        orig_rows = np.where(is_orig & np.all((newpos0 > lo_) & (newpos0 < hi_), axis=1))[0]
        P_orig = newpos0[orig_rows]
        log(f"vulva: carry keys: {int(is_orig.sum())} orig / {int((~is_orig).sum())} new verts, {len(orig_rows)} orig in box {lo_.round(3)}..{hi_.round(3)}")
        new_rows = np.where(~is_orig)[0]
        nb_idx = np.zeros((len(new_rows), 8), np.int64); nb_w = np.zeros((len(new_rows), 8), np.float32)
        for a in range(0, len(new_rows), 2048):
            q = newpos0[new_rows[a:a + 2048]]
            d = np.linalg.norm(q[:, None, :] - P_orig[None, :, :], axis=2)
            ii = np.argpartition(d, 8, axis=1)[:, :8]; dd = np.take_along_axis(d, ii, 1)
            w = 1.0 / np.maximum(dd, 1e-5) ** 2; w /= w.sum(1, keepdims=True)
            nb_idx[a:a + 2048] = oidx[orig_rows[ii]]; nb_w[a:a + 2048] = w
        body.shape_key_add(name="Basis", from_mix=False)
        base = np.empty(len(me.vertices) * 3, np.float32); me.vertices.foreach_get("co", base); base = base.reshape(-1, 3)
        for name, dlt, val in keys:
            D = np.zeros_like(base)
            D[is_orig] = dlt[oidx[is_orig]]
            D[new_rows] = (dlt[nb_idx] * nb_w[..., None]).sum(1)
            kb = body.shape_key_add(name=name, from_mix=False); kb.data.foreach_set("co", (base + D).ravel()); kb.value = val
    log(f"vulva: done — body {n0} -> {len(me.vertices)} verts, {len(keys)} shape keys carried")
    return {"s_A": s_A, "s_P": s_P, "s_N": s_N}
