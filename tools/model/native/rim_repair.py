# Island rims: the colour along every UV island's border made the island's own, so a seam never draws a line.
#   python3 tools/model/native/rim_repair.py <albedo.jpg> [...] --glb <role>.glb [--ymin 1.40] [--rim 12] [--pad 24]
# Marcus' albedos carry a dark red rim 8-10 texels wide (4K) along the head and neck islands' borders: the MakeHuman
# source painted the throat dark red with hard edges, and the tone fixes after it lifted the interior but not the
# last texels at the border (normalized convolution gives them little weight). Where the islands meet, the rim is
# sampled on both sides of the seam: in every light a thin dark red line from under the ear to under the chin.
# Per island (at 4K, triangles above --ymin only): the colour of a ring just inside the rim is low-passed and pushed
# outward over the rim and the padding (iterated normalized convolution); inside the rim the low band is replaced by
# it (high-frequency detail kept), and the padding takes it too, so mip levels never pull the background in.
# Mucosa islands (mouth, nostrils) keep their borders. Run before tone_seams.py (which then matches the seams).
import argparse, os, sys
import numpy as np
from PIL import Image
from scipy import ndimage
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "male"))
from neck_tone import load_glb

R4 = 4096


def raster_islands(pos, uv, tri, ymin):
    """island id per 4K texel for the triangles above ymin (UV corners shared = one island), -1 elsewhere"""
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
        parent[find(t[1])] = find(t[0]); parent[find(t[2])] = find(t[0])
    root = np.array([find(v) for v in range(len(uv))])
    isl = np.full((R4, R4), -1, np.int64)
    px = uv * R4 - 0.5
    keep = pos[tri].mean(1)[:, 1] > ymin
    for t in tri[keep]:
        p = px[t]
        x0, x1 = max(int(np.floor(p[:, 0].min())), 0), min(int(np.ceil(p[:, 0].max())), R4 - 1)
        y0, y1 = max(int(np.floor(p[:, 1].min())), 0), min(int(np.ceil(p[:, 1].max())), R4 - 1)
        if x1 < x0 or y1 < y0: continue
        xs, ys = np.meshgrid(np.arange(x0, x1 + 1), np.arange(y0, y1 + 1))
        d = (p[1, 1] - p[2, 1]) * (p[0, 0] - p[2, 0]) + (p[2, 0] - p[1, 0]) * (p[0, 1] - p[2, 1])
        if abs(d) < 1e-12: continue
        l0 = ((p[1, 1] - p[2, 1]) * (xs - p[2, 0]) + (p[2, 0] - p[1, 0]) * (ys - p[2, 1])) / d
        l1 = ((p[2, 1] - p[0, 1]) * (xs - p[2, 0]) + (p[0, 0] - p[2, 0]) * (ys - p[2, 1])) / d
        m = (l0 >= -0.01) & (l1 >= -0.01) & (1 - l0 - l1 >= -0.01)
        isl[ys[m], xs[m]] = root[t[0]]
    return isl


def nconv(img, w, sigma):
    num = ndimage.gaussian_filter(img * w[..., None], (sigma, sigma, 0))
    den = ndimage.gaussian_filter(w, sigma)
    return num / np.maximum(den, 1e-6)[..., None], den


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("albedo", nargs="+")
    ap.add_argument("--glb", required=True)
    ap.add_argument("--ymin", type=float, default=1.40)
    ap.add_argument("--rim", type=float, default=12.0, help="rim width replaced (4K texels)")
    ap.add_argument("--pad", type=float, default=24.0, help="padding filled outside the island (4K texels)")
    a = ap.parse_args()
    pos, uv, tri = load_glb(a.glb)
    isl = raster_islands(pos, uv, tri, a.ymin)
    ids, counts = np.unique(isl[isl >= 0], return_counts=True)
    ids = ids[counts > 4000]                                   # islands big enough to matter
    for f in a.albedo:
        im = np.asarray(Image.open(f).convert("RGB")).astype(np.float32)
        out = im.copy()
        big = ids[np.argmax([(isl == k).sum() for k in ids])]
        rg_big = np.median(im[isl == big][:, 0] / np.maximum(im[isl == big][:, 1], 1.0))
        for k in ids:
            m = isl == k
            rg = np.median(im[m][:, 0] / np.maximum(im[m][:, 1], 1.0))
            if rg > 1.22 * rg_big: continue                    # mucosa keeps its border
            ys, xs = np.nonzero(m)
            y0, y1 = max(ys.min() - 64, 0), min(ys.max() + 65, R4); x0, x1 = max(xs.min() - 64, 0), min(xs.max() + 65, R4)
            M = m[y0:y1, x0:x1]; C = im[y0:y1, x0:x1]
            d = ndimage.distance_transform_edt(M)                # inside: distance to the border
            near = ((d >= a.rim) & (d < a.rim + 30)).astype(np.float32)   # the interior just inside the rim
            # the interior's colour, pushed outward: normalized convolution at growing radii, nearer data first
            ext = np.zeros_like(C); have = np.zeros(M.shape, bool)
            for sg in (3.0, 6.0, 12.0, 24.0, 48.0):
                v, den = nconv(C, near, sg)
                take = (den > 1e-3) & ~have
                ext[take] = v[take]; have |= take
            # inside the rim: the low band from the interior, the detail kept (high = C / lowpass(C))
            low, _ = nconv(C, M.astype(np.float32), 3.0)
            hi = C / np.maximum(low, 1.0)
            t = np.clip((a.rim + 4.0 - d) / 4.0, 0.0, 1.0)          # 1 inside the rim, 0 four texels further in
            t = (t * t * (3 - 2 * t))[..., None] * M[..., None]
            rimc = ext * np.clip(hi, 0.85, 1.18)
            blk = C * (1 - t) + rimc * t
            # padding outside the island (not another island's texels): the extended colour, no detail
            outside = ~M & (ndimage.distance_transform_edt(~M) <= a.pad) & (isl[y0:y1, x0:x1] < 0)
            blk[outside] = ext[outside]
            o = out[y0:y1, x0:x1]
            o[M] = blk[M]; o[outside] = blk[outside]
            out[y0:y1, x0:x1] = o
            ch = np.abs(blk - C).max(-1)[M & (d < a.rim + 2)]
            print(f"{os.path.basename(f)}: island {k} ({int(m.sum())} texels at 4K): rim texels {int((M & (d < a.rim)).sum())}, "
                  f"mean change {ch.mean():.1f}/255, max {ch.max():.0f}/255")
        Image.fromarray(np.clip(out + 0.5, 0, 255).astype(np.uint8)).save(f, quality=88, optimize=True)


if __name__ == "__main__":
    main()
