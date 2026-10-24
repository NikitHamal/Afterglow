# Marcus' under-chin and throat, at the source of the shipped albedos: no red band down the neck.
#   python3 tools/model/male/neck_tone.py native/assets/characters [marcus.glb]   (all three painted tones, in place)
# The MakeHuman base skins under paint.py paint the underside of the jaw and the top of the throat darker and much
# redder than the neck around them (R/G 1.8-2.1 against 1.5-1.7), with hard edges along the UV islands that meet
# there (the head island's chin edge, the body island's throat strip). In any light it reads as a red band down the
# side of the neck under the ear. As in face_tone.py, only the large-scale tone is replaced (frequency separation:
# a multiplicative correction of the low-pass colour), here by the tone of the surrounding neck skin carried in by
# normalized convolution inside each island; pores and shading stay as painted. The region is chosen in 3D (texels
# rasterised from the body mesh with their bind positions): under the chin / upper throat only, never the lips.
# Run it once, on albedos that have not been through it (it measures redness against the neck around it, so a
# second run finds little to change, but keep the tool idempotent by not chaining it).
import json, struct, sys
import numpy as np
from PIL import Image

WORK = 1024           # working resolution of the low-pass field (the albedos are 4K)
SIGMA = 10.0          # low-pass radius at WORK (~40 px at 4K)


def load_glb(path):
    b = open(path, "rb").read()
    off, js, bn = 12, None, None
    while off < len(b):
        cl, ct = struct.unpack_from("<II", b, off); off += 8
        if ct == 0x4E4F534A: js = json.loads(b[off:off + cl])
        elif ct == 0x004E4942: bn = b[off:off + cl]
        off += cl
    def acc(i):
        a = js["accessors"][i]; bv = js["bufferViews"][a["bufferView"]]
        dt = np.dtype({5126: np.float32, 5123: np.uint16, 5125: np.uint32}[a["componentType"]])
        n = {"SCALAR": 1, "VEC2": 2, "VEC3": 3}[a["type"]]
        o, st = bv.get("byteOffset", 0) + a.get("byteOffset", 0), bv.get("byteStride", 0) or dt.itemsize * n
        rows = np.frombuffer(bn, np.uint8, st * (a["count"] - 1) + dt.itemsize * n, o)
        idx = np.arange(a["count"])[:, None] * st + np.arange(dt.itemsize * n)[None, :]
        return rows[idx].copy().view(dt).reshape(-1, n)
    pr = js["meshes"][0]["primitives"][0]
    pos = acc(pr["attributes"]["POSITION"]).astype(np.float64)
    uv = acc(pr["attributes"]["TEXCOORD_0"]).astype(np.float64)
    tri = acc(pr["indices"]).reshape(-1, 3).astype(np.int64)
    return pos, uv, tri


def rasterize(pos, uv, tri, R):
    """per texel of an R x R grid: bind position (nan outside every triangle) and a triangle-island id"""
    P = np.full((R, R, 3), np.nan)
    isl = np.full((R, R), -1, np.int64)
    # islands: triangles connected through shared UV corners
    key = np.round(uv * 20000).astype(np.int64)
    corner = key[:, 0] * 40000 + key[:, 1]
    parent = np.arange(len(uv))
    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]; a = parent[a]
        return a
    first = {}
    for v, c in enumerate(corner):
        if c in first: parent[find(v)] = find(first[c])
        else: first[c] = v
    for t in tri:
        a, b = find(t[0]), find(t[1]); parent[b] = a
        c = find(t[2]); parent[c] = find(t[0])
    root = np.array([find(v) for v in range(len(uv))])
    px = uv * R - 0.5
    for t in tri:
        p = px[t]
        x0, x1 = int(np.floor(p[:, 0].min())), int(np.ceil(p[:, 0].max()))
        y0, y1 = int(np.floor(p[:, 1].min())), int(np.ceil(p[:, 1].max()))
        x0, y0 = max(x0, 0), max(y0, 0); x1, y1 = min(x1, R - 1), min(y1, R - 1)
        if x1 < x0 or y1 < y0: continue
        xs, ys = np.meshgrid(np.arange(x0, x1 + 1), np.arange(y0, y1 + 1))
        d = (p[1, 1] - p[2, 1]) * (p[0, 0] - p[2, 0]) + (p[2, 0] - p[1, 0]) * (p[0, 1] - p[2, 1])
        if abs(d) < 1e-12: continue
        l0 = ((p[1, 1] - p[2, 1]) * (xs - p[2, 0]) + (p[2, 0] - p[1, 0]) * (ys - p[2, 1])) / d
        l1 = ((p[2, 1] - p[0, 1]) * (xs - p[2, 0]) + (p[0, 0] - p[2, 0]) * (ys - p[2, 1])) / d
        l2 = 1 - l0 - l1
        m = (l0 >= -0.02) & (l1 >= -0.02) & (l2 >= -0.02)
        if not m.any(): continue
        q = l0[m, None] * pos[t[0]] + l1[m, None] * pos[t[1]] + l2[m, None] * pos[t[2]]
        P[ys[m], xs[m]] = q
        isl[ys[m], xs[m]] = root[t[0]]
    return P, isl


def gauss(img, sigma):
    """separable gaussian (reflect edges)"""
    r = int(3 * sigma)
    k = np.exp(-0.5 * (np.arange(-r, r + 1) / sigma) ** 2); k /= k.sum()
    out = img
    for ax in (0, 1):
        pad = [(0, 0)] * out.ndim; pad[ax] = (r, r)
        q = np.pad(out, pad, mode="reflect")
        acc = np.zeros_like(out)
        for i, w in enumerate(k):
            sl = [slice(None)] * out.ndim; sl[ax] = slice(i, i + out.shape[ax])
            acc += w * q[tuple(sl)]
        out = acc
    return out


def main():
    d = sys.argv[1] if len(sys.argv) > 1 else "native/assets/characters"
    glb = sys.argv[2] if len(sys.argv) > 2 else d + "/marcus.glb"
    pos, uv, tri = load_glb(glb)
    P, isl = rasterize(pos, uv, tri, WORK)
    cov = ~np.isnan(P[..., 0])
    y, x, z = P[..., 1], P[..., 0], P[..., 2]
    # the lips stay out: the highest red texels of the face are the mouth; the zone ends well below it
    zone = cov & (y > 1.50) & (y < 1.632) & (np.abs(x) < 0.085)
    neck = cov & (y > 1.44) & (y < 1.56) & (np.abs(x) < 0.09)
    print("zone texels", int(zone.sum()), "neck ring texels", int(neck.sum()))
    for tone in ("honey", "caramel", "deep"):
        f = f"{d}/marcus_skin_{tone}.jpg"
        im = Image.open(f).convert("RGB")
        full = np.asarray(im).astype(np.float64)
        small = np.asarray(im.resize((WORK, WORK), Image.BOX)).astype(np.float64)
        # low-pass colour inside the islands (normalized convolution: padding never bleeds in)
        L = gauss(small * cov[..., None], SIGMA) / np.maximum(gauss(cov.astype(np.float64), SIGMA), 1e-6)[..., None]
        rg = L[..., 0] / np.maximum(L[..., 1], 1e-3)
        base = np.median(rg[neck])
        excess = np.clip((rg - base - 0.04) / 0.14, 0.0, 1.0)
        w = zone * excess
        w = gauss(w, 3.0) * cov                                  # soft edge
        good = cov & (w < 0.02) & (np.abs(x) < 0.12) & (y > 1.40) & (y < 1.70)
        # the neck's own tone carried into the red region, island by island
        T = np.zeros_like(L)
        for k in np.unique(isl[w > 0.02]):
            m = isl == k
            g = (good & m).astype(np.float64)
            num = gauss(small * g[..., None], 3 * SIGMA)
            den = gauss(g, 3 * SIGMA)
            T[m] = (num / np.maximum(den, 1e-6)[..., None])[m]
        T = np.where((w > 0.02)[..., None], T, L)
        fac = np.where(cov[..., None], (L + w[..., None] * (T - L)) / np.maximum(L, 1.0), 1.0)
        fac = np.clip(fac, 0.6, 1.6)
        F = np.stack([np.asarray(Image.fromarray(fac[..., c].astype(np.float32), "F").resize(im.size, Image.BILINEAR)) for c in range(3)], -1)
        out = np.clip(full * F, 0, 255).astype(np.uint8)
        Image.fromarray(out).save(f, quality=88, optimize=True)   # about the original encoding (size stays put)
        changed = (np.abs(fac - 1.0).max(-1) > 0.02)
        print(f"{tone}: neck R/G {base:.3f}, red region max R/G {rg[zone].max():.3f}, texels changed {int(changed.sum())}")


if __name__ == "__main__":
    main()
