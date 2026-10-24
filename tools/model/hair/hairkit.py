# Hair kit: card hair grown on a character's own head in game space (a <base|model>_game.blend: MMD rig, real scale),
# shared by every style (styles.py) — head landmarks, an irregular hairline, Poisson roots (denser at the hairline),
# strands that hug the skull or fall from it, clumping, cards (U across, V root -> tip), multi-card locks on the rig's
# simulated chains, a scalp cap, a strand atlas, weights, weight_up/dn morphs, and the glTF writer.
# Conventions (docs/NATIVE_CONTENT.md, docs/LOOK_NOTES.md): glTF V = 1 at the root, 0 at the tip; atlas 8 columns;
# hair MASK 0.42 double-sided; colour = baseColorFactor (espresso) x the game's tint; COLOR_0.a = coverage.
import bpy, math, json, struct, io
import numpy as np
from mathutils.bvhtree import BVHTree
from mathutils.kdtree import KDTree
from mathutils import Vector
from PIL import Image

HEAD = "頭"
HAIR_LIN = (0.0423114113509655, 0.015208514407277107, 0.008568125776946545)   # authored espresso (linear)

def ss(e0, e1, x):
    t = np.clip((np.asarray(x, np.float64) - e0) / (e1 - e0), 0, 1); return t * t * (3 - 2 * t)
def nrm(v):
    n = np.linalg.norm(v, axis=-1, keepdims=True); return v / np.maximum(n, 1e-12)
def tangent(v, n):
    v = v - n * (v @ n); L = np.linalg.norm(v); return v / L if L > 1e-12 else v

class Head:
    """the character's head at rest (shape keys off): landmarks, scalp, BVH, hairline"""
    def __init__(self, body_name, eyes_name, rig_name, rng):
        self.rng = rng
        self.body = bpy.data.objects[body_name]; self.rig = bpy.data.objects[rig_name]
        me = self.body.data
        if me.shape_keys:
            for kb in me.shape_keys.key_blocks: kb.value = 0.0
        V = np.empty(len(me.vertices) * 3, np.float32); me.vertices.foreach_get("co", V)
        if me.shape_keys:
            me.shape_keys.key_blocks[0].data.foreach_get("co", V)
        self.V = V.reshape(-1, 3).astype(np.float64)
        self.keys = {}
        if me.shape_keys:
            for kb in me.shape_keys.key_blocks[1:]:
                a = np.empty(len(me.vertices) * 3, np.float32); kb.data.foreach_get("co", a)
                self.keys[kb.name] = a.reshape(-1, 3).astype(np.float64) - self.V
        gi = self.body.vertex_groups[HEAD].index
        w = np.zeros(len(me.vertices))
        for v in me.vertices:
            for g in v.groups:
                if g.group == gi: w[v.index] = g.weight
        self.wh = w
        eo = bpy.data.objects[eyes_name]
        E = np.empty(len(eo.data.vertices) * 3, np.float32); eo.data.vertices.foreach_get("co", E); E = E.reshape(-1, 3)
        self.EL = E[E[:, 0] > 0].mean(0).astype(np.float64); self.ER = E[E[:, 0] < 0].mean(0).astype(np.float64)
        self.EC = (self.EL + self.ER) / 2
        hv = self.V[self.wh > 0.5]
        self.TOP_Z = hv[:, 2].max()
        chin = hv[(np.abs(hv[:, 0]) < 0.015) & (hv[:, 1] < self.EC[1])]
        self.CHIN_Z = chin[:, 2].min()
        self.k = (self.TOP_Z - self.CHIN_Z) / 0.2215          # 1.0 for Vivian's game head
        k = self.k
        self.Ch = np.array([0.0, self.EC[1] + 0.072 * k, self.EC[2] + 0.012 * k])     # skull centre
        # ears: the head's lateral extremes in the band from eye level to 5 cm below
        band = hv[(hv[:, 2] < self.EC[2] + 0.005) & (hv[:, 2] > self.EC[2] - 0.05 * k) & (hv[:, 1] > self.EC[1] + 0.03 * k)]
        self.EARC = []
        for s in (1, -1):
            b = band[np.sign(band[:, 0]) == s]
            ext = b[np.abs(b[:, 0]) > np.percentile(np.abs(b[:, 0]), 90)]
            self.EARC.append(ext.mean(0))
        self.EAR_R = 0.036 * k
        polys = [tuple(p.vertices) for p in me.polygons if all(self.wh[i] > 0.05 for i in p.vertices) or
                 all(self.V[i][2] > self.CHIN_Z - 0.25 * k for i in p.vertices)]
        self.bvh = BVHTree.FromPolygons([tuple(v) for v in self.V], polys)
        self.kd = KDTree(len(self.V))
        for i, p in enumerate(self.V):
            if p[2] > self.CHIN_Z - 0.3 * k: self.kd.insert(p, i)
        self.kd.balance()
        self.hl_noise = rng.normal(0, 1, 64)
        print(f"[hairkit] head k {k:.3f} centre {self.Ch.round(3)} eyes {self.EC.round(3)} top {self.TOP_Z:.3f} chin {self.CHIN_Z:.3f}", flush=True)

    def face_half(self, z):
        """half-width of the face/head at height z, front half (for hair that must hang beside the cheeks)"""
        if not hasattr(self, "_fh"):
            hv = self.V[(self.wh > 0.3) & (self.V[:, 1] < self.EC[1] + 0.05 * self.k)]
            zs = np.linspace(self.CHIN_Z - 0.03, self.TOP_Z, 48)
            hw = [np.abs(hv[np.abs(hv[:, 2] - z_) < 0.006][:, 0]).max() if (np.abs(hv[:, 2] - z_) < 0.006).any() else 0.05 for z_ in zs]
            self._fh = (zs, np.maximum.accumulate(np.array(hw)[::-1])[::-1])   # never narrower than below (jaw -> cheek)
        return float(np.interp(z, *self._fh))
    def near(self, p):
        loc, n, _, d = self.bvh.find_nearest(Vector(tuple(p))); return np.array(loc), np.array(n), d
    def phi(self, p):
        p = np.asarray(p); return np.arctan2(p[..., 0] - self.Ch[0], -(p[..., 1] - self.Ch[1]))   # 0 front, +-pi back
    # hairline height above the eyes by angle round the head (Vivian's, scaled), with a little irregularity
    HL_A = [0.0, 0.35, 0.70, 1.00, 1.25, 1.45, 1.75, 2.05, 2.50, math.pi]
    HL_Z = [0.070, 0.064, 0.048, 0.032, 0.022, 0.016, 0.004, -0.045, -0.078, -0.088]
    def hairline_z(self, p, irregular=True):
        a = np.abs(self.phi(p))
        z = self.EC[2] + np.interp(a, self.HL_A, self.HL_Z) * self.k
        if irregular:
            ph = self.phi(p)
            j = (ph + math.pi) / (2 * math.pi) * 63.999
            i0 = np.floor(j).astype(int); t = j - i0
            nz = self.hl_noise[i0 % 64] * (1 - t) + self.hl_noise[(i0 + 1) % 64] * t
            z = z + nz * 0.0018 * self.k
        return z
    def ear_dist(self, p):
        p = np.asarray(p)
        return np.minimum(np.linalg.norm(p - self.EARC[0], axis=-1), np.linalg.norm(p - self.EARC[1], axis=-1))
    def on_scalp(self, p, margin=0.0):
        return (p[..., 2] > self.hairline_z(p) + margin) & (self.ear_dist(p) > self.EAR_R + 0.004 * self.k)

    def scalp_tris(self, margin=0.0):
        sv = (self.wh > 0.5) & self.on_scalp(self.V, margin - 0.006 * self.k)
        tris = []
        for p in self.body.data.polygons:
            vs = list(p.vertices)
            if all(sv[i] for i in vs):
                tris.append((vs[0], vs[1], vs[2]))
                if len(vs) == 4: tris.append((vs[0], vs[2], vs[3]))
        return np.array(tris)

    def roots(self, r_far, r_line, band, n_cand=90000, keep=None):
        """Poisson-disk roots on the scalp: radius r_line within `band` of the hairline, r_far beyond"""
        tris = self.scalp_tris(); V = self.V
        A, B, C = V[tris[:, 0]], V[tris[:, 1]], V[tris[:, 2]]
        area = 0.5 * np.linalg.norm(np.cross(B - A, C - A), axis=1)
        pick = self.rng.choice(len(tris), size=n_cand, p=area / area.sum())
        r1, r2 = np.sqrt(self.rng.random(n_cand)), self.rng.random(n_cand)
        P = A[pick] * (1 - r1)[:, None] + B[pick] * (r1 * (1 - r2))[:, None] + C[pick] * (r1 * r2)[:, None]
        P = P[self.on_scalp(P)]
        if keep is not None: P = P[keep(P)]
        d_line = P[:, 2] - self.hairline_z(P)
        rad = r_line + (r_far - r_line) * ss(0.0, band, d_line)
        order = np.argsort(d_line)                      # hairline first: the dense band gets its points
        cell = r_line; grid = {}; out = []
        for i in order:
            p = P[i]; r = rad[i]; key = tuple((p // cell).astype(int)); ok = True
            reach = int(math.ceil(r / cell))
            for dx in range(-reach, reach + 1):
                for dy in range(-reach, reach + 1):
                    for dz in range(-reach, reach + 1):
                        for j in grid.get((key[0] + dx, key[1] + dy, key[2] + dz), ()):
                            if np.sum((P[j] - p) ** 2) < (0.5 * (r + rad[j])) ** 2: ok = False; break
                        if not ok: break
                    if not ok: break
                if not ok: break
            if ok: out.append(i); grid.setdefault(key, []).append(i)
        return P[out]

    def morph_deltas(self, pts, names=("weight_up", "weight_dn")):
        """hair follows the head: each point takes the delta of its nearest head vertex"""
        out = []
        idx = np.array([self.kd.find(tuple(p))[1] for p in pts])
        for n in names:
            out.append(self.keys[n][idx] if n in self.keys else np.zeros((len(pts), 3)))
        return out

    def chain(self, n):
        """a simulated chain's guide points (bone heads a..d + the z tip) in rest"""
        bones = self.rig.data.bones
        pts = [np.array(bones[f"髪.{n}.{s}"].head_local) for s in "abcd"]
        pts.append(np.array(bones[f"髪.{n}.z"].head_local))
        return np.array(pts)
    def has_chains(self):
        return f"髪.0.a" in self.rig.data.bones

# ---------------------------------------------------------------------------------------------- geometry builder
class Builder:
    def __init__(self, head):
        self.h = head; self.rng = head.rng
        self.P, self.UV, self.W, self.A = [], [], [], []   # positions, uvs, weight dicts, coverage
        self.F = []; self.FM = []                          # quads/tris, material per face (0 cards, 1 cap)
    def card(self, P, Bv, w, col, wfun, alpha=1.0, twist_sd=0.0, outv=None, ncol=8, v0=0.0):
        nseg = len(P) - 1; uu = np.linspace(0, 1, nseg + 1); vv = v0 + (1.0 - v0) * uu
        if twist_sd > 0 and outv is not None:
            tw = np.radians(self.rng.normal(0, twist_sd)) + np.radians(self.rng.normal(0, twist_sd * 0.5)) * uu
            Bv = nrm(Bv * np.cos(tw)[:, None] + outv * np.sin(tw)[:, None])
        u0, u1 = (col + 0.03) / ncol, (col + 0.97) / ncol
        base = len(self.P)
        for k in range(nseg + 1):
            wk = wfun(uu[k])
            self.P += [P[k] - Bv[k] * w[k] / 2, P[k] + Bv[k] * w[k] / 2]
            self.UV += [(u0, vv[k]), (u1, vv[k])]; self.W += [wk, wk]; self.A += [alpha, alpha]
        for k in range(nseg):
            a = base + 2 * k; self.F.append((a, a + 1, a + 3, a + 2)); self.FM.append(0)
    def strand_cards(self, S, width, cols, wfun=None, alpha=1.0, taper=0.30, twist_sd=0.0, v0=0.0):
        h = self.h; wfun = wfun or (lambda u: {HEAD: 1.0})
        T = nrm(np.gradient(S, axis=1))
        for si in range(len(S)):
            P = S[si]; out = np.array([h.near(p)[1] for p in P]); Bv = nrm(np.cross(T[si], out))
            w = width * (1 - taper * np.linspace(0, 1, len(P))) * self.rng.uniform(0.85, 1.15)
            self.card(P, Bv, w, int(self.rng.choice(cols)), wfun, alpha=alpha, twist_sd=twist_sd, outv=out, v0=v0)
    def lock(self, G, cards, radius, width, cols, wfun, alpha=1.0, spread_tip=1.6, twist_sd=25.0, taper=0.55, v0=0.0):
        """a lock: several thin twisted cards round a guide, fanning a little at the tips"""
        h = self.h
        T = nrm(np.gradient(G, axis=0)); uG = np.linspace(0, 1, len(G))
        out = np.array([h.near(p)[1] for p in G])
        Bv0 = nrm(np.cross(T, out)); Nv = nrm(np.cross(Bv0, T))
        for _ in range(cards):
            a = self.rng.uniform(0, 2 * np.pi); r = self.rng.uniform(0.15, 1.0) * radius
            fan = (0.35 + 0.65 * uG) * (1 + (spread_tip - 1) * uG ** 2)
            P = G + (Bv0 * math.cos(a) + Nv * math.sin(a)) * (r * fan)[:, None]
            cut = self.rng.uniform(0.82, 1.0)                     # uneven ends
            m = max(3, int(round(len(G) * cut)))
            P = P[:m]; Pv = Bv0[:m]; Nn = Nv[:m]
            w = width * (1 - taper * np.linspace(0, 1, m)) * self.rng.uniform(0.75, 1.2)
            self.card(P, Pv, w, int(self.rng.choice(cols)), wfun, alpha=alpha, twist_sd=twist_sd, outv=Nn, v0=v0)
    def cap(self, lift, above, uv_fn, fade_from=None, fade_len=0.010):
        """the scalp faces lifted off the skin, subdivided twice, as the opaque under-layer. It reaches below where it
        shows (`above`) and fades in from `fade_from` over `fade_len` above the hairline, so its visible edge is the
        smooth fade contour, never the head mesh's triangle border."""
        import bmesh
        h = self.h; V = h.V
        if fade_from is None: fade_from = above
        sv = (h.wh > 0.5) & h.on_scalp(V, min(above, fade_from - 0.008 * h.k))
        lo = min(above, fade_from - 0.008 * h.k)
        faces = [tuple(p.vertices) for p in h.body.data.polygons
                 if all(sv[i] and V[i][2] > h.hairline_z(V[i], irregular=False) + lo for i in p.vertices)]
        ids = sorted({i for f in faces for i in f})
        bm = bmesh.new(); bv = {i: bm.verts.new(tuple(V[i])) for i in ids}
        for f in faces:
            try: bm.faces.new([bv[i] for i in f])
            except ValueError: pass
        bmesh.ops.subdivide_edges(bm, edges=bm.edges[:], cuts=2, use_grid_fill=True)
        bm.verts.ensure_lookup_table(); bm.faces.ensure_lookup_table()
        base = len(self.P)
        for v in bm.verts:
            pv = np.array(v.co); loc, n, _ = h.near(pv); pv = loc + n * lift
            # the cap fades out over its first centimetre above the hairline (no hard edge where it meets the skin)
            fade = float(0.02 + 0.98 * ss(fade_from, fade_from + fade_len * h.k, pv[2] - h.hairline_z(pv, irregular=False)))
            self.P.append(pv); self.UV.append(uv_fn(pv)); self.W.append({HEAD: 1.0}); self.A.append(fade)
        for f in bm.faces: self.F.append(tuple(base + v.index for v in f.verts)); self.FM.append(1)
        n = len(bm.faces); bm.free(); return n

    def arrays(self):
        """triangulated arrays per material, glTF frame (Y up), glTF UV (V root 1 -> tip 0 for cards)"""
        P = np.array(self.P); UV = np.array(self.UV, np.float64); A = np.array(self.A)
        out = []
        for mat in (0, 1):
            fs = [f for f, m in zip(self.F, self.FM) if m == mat]
            if not fs: out.append(None); continue
            used = sorted({i for f in fs for i in f}); remap = {o: n for n, o in enumerate(used)}
            tris = []
            for f in fs:
                g = [remap[i] for i in f]
                tris.append((g[0], g[1], g[2]))
                if len(g) == 4: tris.append((g[0], g[2], g[3]))
            idx = np.array(used)
            pos = P[idx]; uv = UV[idx].copy()
            if mat == 0: uv[:, 1] = 1.0 - uv[:, 1]            # glTF: root at V = 1
            else: uv[:, 1] = -uv[:, 1]
            # normals: area-weighted face normals (the shader bends card normals round the skull anyway)
            nrmv = np.zeros_like(pos); T = np.array(tris)
            fn = np.cross(pos[T[:, 1]] - pos[T[:, 0]], pos[T[:, 2]] - pos[T[:, 0]])
            for c in range(3): np.add.at(nrmv, T[:, c], fn)
            # cap normals face out of the skull; cards: whichever side faces out
            out_dir = nrm(pos - self.h.Ch)
            flip = (nrmv * out_dir).sum(1) < 0
            nrmv[flip] *= -1
            nrmv = nrm(nrmv + out_dir * 1e-6)
            out.append(dict(pos=pos, nrm=nrmv, uv=uv, alpha=A[idx], w=[self.W[i] for i in idx], tris=T, src=idx))
        return out

def to_gltf(p): return np.stack([p[..., 0], p[..., 2], -p[..., 1]], -1)
