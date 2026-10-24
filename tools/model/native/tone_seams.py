# Skin tone seams: the large-scale colour of a painted skin albedo made continuous across the body's UV seams.
#   python3 tools/model/native/tone_seams.py <albedo.jpg> [...] --glb <role>.glb [--region head|all] [--report]
# MakeHuman skins (and the per-island fixes painted on them since) carry a different mean tone on each UV island: the
# head island's jaw-and-neck strip meets the body island's throat along a seam where the two disagree by 10-20 % in
# brightness and redness. In any light that reads as a hard-edged band down the side of the neck. Only the low
# frequencies move here (frequency separation: a smooth multiplicative correction), so pores, stubble and painted
# shading stay as they are:
#   1. rasterise the body mesh into UV space at 1024² (bind position + island id per texel), low-pass each island's
#      colour by normalized convolution (no padding or other island bleeds in);
#   2. along every seam (vertices at one 3D position with different UVs), each side's low-pass colour; the target on
#      the seam is their mean, each side's log-correction is target / own;
#   3. per island, those corrections are spread from the seam into the island (normalized convolution) and faded to
#      nothing over ~FALLOFF_M of skin, so both sides meet the same colour on the seam and keep their own further in.
# A second run measures the seams matched (a few %) and changes almost nothing.
import argparse, os, sys
import numpy as np
from PIL import Image
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "male"))
from neck_tone import load_glb, rasterize, gauss   # GLB reader, UV rasteriser, separable gaussian

WORK = 1024
SIGMA = 5.0            # low-pass radius of the measured tone (texels at WORK)
FALLOFF_M = 0.05       # how far into each island a seam correction reaches (metres of skin)


def seam_pairs(pos, uv):
    """vertex pairs that are one point of the surface at different UV corners (the UV seams)"""
    key = np.round(pos * 1e5).astype(np.int64)
    order = np.lexsort((key[:, 2], key[:, 1], key[:, 0]))
    ks = key[order]
    brk = np.nonzero(np.any(np.diff(ks, axis=0) != 0, axis=1))[0] + 1
    pairs = []
    for g in np.split(order, brk):
        for i in range(len(g)):
            for j in range(i + 1, len(g)):
                if np.abs(uv[g[i]] - uv[g[j]]).max() > 1e-4: pairs.append((g[i], g[j]))
    return np.array(pairs, np.int64).reshape(-1, 2)


def texel_of(uvv, cov, win=4):
    """the covered texel nearest to a UV point (UV islands never overlap: it is the vertex's own island)"""
    cx, cy = uvv[0] * WORK - 0.5, uvv[1] * WORK - 0.5
    x0, y0 = int(round(cx)), int(round(cy))
    best, bd = None, 1e9
    for y in range(max(0, y0 - win), min(WORK, y0 + win + 1)):
        for x in range(max(0, x0 - win), min(WORK, x0 + win + 1)):
            if cov[y, x]:
                d = (x - cx) ** 2 + (y - cy) ** 2
                if d < bd: best, bd = (y, x), d
    return best


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("albedo", nargs="+")
    ap.add_argument("--glb", required=True)
    ap.add_argument("--region", default="head", help="head: seams between 1.30 m and the crown (the visible band); all")
    ap.add_argument("--ymin", type=float, default=1.30)
    ap.add_argument("--report", action="store_true")
    ap.add_argument("--iters", type=int, default=4)
    a = ap.parse_args()
    pos, uv, tri = load_glb(a.glb)
    P, isl = rasterize(pos, uv, tri, WORK)
    cov = ~np.isnan(P[..., 0])
    pairs = seam_pairs(pos, uv)
    if a.region == "head":
        pairs = pairs[pos[pairs[:, 0], 1] > a.ymin]
    islands = np.unique(isl[cov])
    # mucosa keeps its colour: the mouth's inside and the nostrils are islands of their own, far redder than skin;
    # their rims are seams that must stay as painted (measured on the first albedo)
    ref = np.asarray(Image.open(a.albedo[0]).convert("RGB").resize((WORK, WORK), Image.BOX)).astype(np.float64)
    rg = {k: float(np.median(ref[isl == k][:, 0] / np.maximum(ref[isl == k][:, 1], 1.0))) for k in islands}
    big = max(islands, key=lambda k: int((isl == k).sum()))
    mucosa = {k for k in islands if rg[k] > 1.22 * rg[big]}
    pts = []
    for va, vb in pairs:
        ta, tb = texel_of(uv[va], cov), texel_of(uv[vb], cov)
        if ta is None or tb is None or ta == tb: continue
        if isl[ta] in mucosa or isl[tb] in mucosa: continue
        pts.append((ta, tb))
    print(f"seam pairs {len(pairs)}, sampled {len(pts)} ({a.region}); mucosa islands kept as painted: {len(mucosa)}")
    # metres per texel, per island (bind positions of neighbouring texels)
    tm = {}
    for k in islands:
        m = isl == k
        d = np.linalg.norm(P[1:, :] - P[:-1, :], axis=-1)[m[1:, :] & m[:-1, :]]
        tm[k] = float(np.median(d)) if len(d) else 0.0015
    for f in a.albedo:
        im = Image.open(f).convert("RGB")
        full = np.asarray(im).astype(np.float64)
        small0 = np.asarray(im.resize((WORK, WORK), Image.BOX)).astype(np.float64)
        total = np.zeros_like(small0)
        for it in range(1 if a.report else a.iters):
            small = small0 * np.exp(total)
            corr, mis = correction(small, isl, cov, islands, pts, tm)
            if it == 0: print(f"{os.path.basename(f)}: seam tone mismatch median {np.median(mis) * 100:.1f} %, p95 "
                              f"{np.percentile(mis, 95) * 100:.1f} %, max {mis.max() * 100:.1f} %")
            total = total + corr
        if a.report: continue
        small = small0 * np.exp(total)
        _, mis = correction(small, isl, cov, islands, pts, tm)
        print(f"  after {a.iters} passes: median {np.median(mis) * 100:.1f} %, p95 {np.percentile(mis, 95) * 100:.1f} %, "
              f"max {mis.max() * 100:.1f} %")
        fac = np.where(cov[..., None], np.exp(np.clip(total, -0.35, 0.35)), 1.0)
        F = np.stack([np.asarray(Image.fromarray(fac[..., c].astype(np.float32), "F").resize(im.size, Image.BILINEAR)) for c in range(3)], -1)
        out = np.clip(full * F + 0.5, 0, 255).astype(np.uint8)
        Image.fromarray(out).save(f, quality=88, optimize=True)
        ch = (np.abs(np.log(fac)).max(-1) > 0.01) & cov
        print(f"  corrected {int(ch.sum())} texels ({ch.sum() / cov.sum() * 100:.1f} % of the skin), "
              f"largest change {np.abs(np.log(fac)).max() * 100:.0f} %")


def correction(small, isl, cov, islands, pts, tm):
    """one pass: the per-texel log-correction that meets each seam's two sides at their mean colour"""
    L = np.zeros_like(small)
    for k in islands:
        m = (isl == k).astype(np.float64)
        Lk = gauss(small * m[..., None], SIGMA) / np.maximum(gauss(m, SIGMA), 1e-6)[..., None]
        L[isl == k] = Lk[isl == k]
    num = np.zeros_like(small); den = np.zeros(isl.shape); mis = []
    for (ya, xa), (yb, xb) in pts:
        ca, cb = L[ya, xa], L[yb, xb]
        t = 0.5 * (ca + cb)
        mis.append(np.abs(ca - cb).max() / max(t.max(), 1.0))
        for (y, x), c in (((ya, xa), ca), ((yb, xb), cb)):
            num[y, x] += np.log(np.maximum(t, 1.0) / np.maximum(c, 1.0)); den[y, x] += 1.0
    corr = np.zeros_like(small)
    for k in islands:
        m = isl == k
        sk = den * m
        if sk.sum() == 0: continue
        sig = max(3.0, FALLOFF_M / tm[k] / 2.5)
        nv = gauss(num * m[..., None], sig)
        dv = gauss(sk, sig)
        val = nv / np.maximum(dv, 1e-12)[..., None]
        ref = np.median(dv[sk > 0])                       # the spread weight right on the seam
        w = np.clip(dv / max(ref, 1e-12), 0.0, 1.0)
        w = w * w * (3 - 2 * w)
        corr[m] = (val * w[..., None])[m]
    return corr, np.array(mis)


if __name__ == "__main__":
    main()
