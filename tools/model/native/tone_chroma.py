# Skin chroma, continuous in 3D: red-pink patches painted into a skin albedo are pulled to the skin around them.
#   python3 tools/model/native/tone_chroma.py <albedo.jpg> [...] --glb <role>.glb [--ymin 1.40 --ymax 1.74] [--report]
# The MakeHuman skins under Marcus' paint carry a pink-red strip under the jaw and down the side of the neck below each
# ear (outside the box-shaped zones face_tone.py / neck_tone.py worked in); in every light it
# read as a red band (measured: a ~1.2 cm strip along the head/body seam under the jaw, R/G 1.70 against 1.58 round
# it). The reference is the 3D median of the skin round each point within 2.8 cm (across UV seams, so it is continuous
# where the islands meet; a strip narrower than that does not pull its own reference); where the low-pass colour is
# redder than it, its chroma (log RGB minus its mean) is pulled to the reference and its luminance lifted (never
# darkened, at most 10 %). Pores, stubble and painted shading above the low-pass stay as painted.
# Mucosa islands (mouth inside, nostrils), the lips (a 3D ball round the mouth), the ears and the scalp are left out.
import argparse, json, os, struct, sys
import numpy as np
from PIL import Image
from scipy.spatial import cKDTree
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "male"))
from neck_tone import load_glb, rasterize, gauss

WORK = 1024
SIGMA = 4.0          # low-pass of the measured colour (texels at WORK)
REF_R = 0.028        # radius of the 3D median reference (m): wider than the painted strips it removes
SMOOTH_S = 0.018     # sigma of the smooth 3D field painted zone borders are blended into (m)
EDGE_C = 0.015       # how far the low band's chroma may stay off that field (log RGB)


def ss(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0); return t * t * (3 - 2 * t)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("albedo", nargs="+")
    ap.add_argument("--glb", required=True)
    ap.add_argument("--ymin", type=float, default=1.40)
    ap.add_argument("--ymax", type=float, default=1.745)
    ap.add_argument("--mouth", default="0,1.675,0.160", help="3D mouth centre (x,y,z, m): the lips stay as painted")
    ap.add_argument("--nose", default="0,1.725,0.150", help="3D nose tip (x,y,z, m): the nose keeps its painted shading")
    ap.add_argument("--iters", type=int, default=3)
    ap.add_argument("--report", action="store_true")
    a = ap.parse_args()
    pos, uv, tri = load_glb(a.glb)
    P, isl = rasterize(pos, uv, tri, WORK)
    cov = ~np.isnan(P[..., 0])
    mouth = np.array([float(v) for v in a.mouth.split(",")])
    nose = np.array([float(v) for v in a.nose.split(",")])
    islands = np.unique(isl[cov])
    first = np.asarray(Image.open(a.albedo[0]).convert("RGB").resize((WORK, WORK), Image.BOX)).astype(np.float64)
    rg = {k: float(np.median(first[isl == k][:, 0] / np.maximum(first[isl == k][:, 1], 1.0))) for k in islands}
    big = max(islands, key=lambda k: int((isl == k).sum()))
    mucosa = np.isin(isl, [k for k in islands if rg[k] > 1.22 * rg[big]])
    y, x, z = P[..., 1], P[..., 0], P[..., 2]
    # where the correction may act (soft in 3D): neck, jaw, lower face; never the lips, the ears, the scalp
    dm = np.linalg.norm(np.nan_to_num(P) - mouth, axis=-1)
    region = cov & ~mucosa & (y > a.ymin - 0.04) & (y < a.ymax + 0.02)
    dn = np.linalg.norm(np.nan_to_num(P) - nose, axis=-1)
    wr = ss(a.ymin - 0.04, a.ymin, y) * ss(a.ymax + 0.02, a.ymax, y) * ss(0.026, 0.040, dm) * ss(0.030, 0.045, dn)
    ear = (np.abs(x) > 0.062) & (y > 1.675) & (z < 0.06)              # the ears (their own translucent red)
    wr = np.where(region & ~ear, wr, 0.0)
    for f in a.albedo:
        im = Image.open(f).convert("RGB")
        full = np.asarray(im).astype(np.float64)
        small0 = np.asarray(im.resize((WORK, WORK), Image.BOX)).astype(np.float64)
        total = np.zeros_like(small0)
        for it in range(1 if a.report else a.iters):
            total = total + one_pass(small0 * np.exp(total), isl, islands, cov, region, wr, P, f, it == 0)
        if a.report: continue
        fac = np.exp(total)
        one_pass(small0 * fac, isl, islands, cov, region, wr, P, f + " (after)", True)
        fac = np.where(cov[..., None], np.clip(fac, 0.75, 1.3), 1.0)
        F = np.stack([np.asarray(Image.fromarray(fac[..., c].astype(np.float32), "F").resize(im.size, Image.BILINEAR)) for c in range(3)], -1)
        out = np.clip(full * F + 0.5, 0, 255).astype(np.uint8)
        Image.fromarray(out).save(f, quality=88, optimize=True)
        print(f"  largest change {np.abs(np.log(fac)).max() * 100:.0f} %")


def one_pass(small, isl, islands, cov, region, wr, P, f, verbose):
    """the log-correction of one pass (0 outside the corrected skin)"""
    if True:
        L = np.zeros_like(small)
        for k in islands:
            m = (isl == k).astype(np.float64)
            Lk = gauss(small * m[..., None], SIGMA) / np.maximum(gauss(m, SIGMA), 1e-6)[..., None]
            L[isl == k] = Lk[isl == k]
        lum = L.mean(-1)
        skin = region & (lum > 0.45 * np.median(lum[region]))
        pts = P[skin]; logc = np.log(np.maximum(L[skin], 1.0))
        ll = logc.mean(-1, keepdims=True)
        chroma = logc - ll
        feat = np.concatenate([chroma, ll], -1)
        # reference: the 3D median of the skin round each point, wider than a painted strip is (a strip does not pull its
        # own reference), at sparse samples interpolated to every texel (inverse distance)
        src = np.arange(0, len(pts), 3)
        tsrc = cKDTree(pts[src])
        smp = np.arange(0, len(pts), 16)
        refs = np.empty((len(smp), 4))
        for j, i in enumerate(smp):
            n = tsrc.query_ball_point(pts[i], REF_R)
            refs[j] = np.median(feat[src[n]], 0) if len(n) > 8 else feat[i]
        d, nn = cKDTree(pts[smp]).query(pts, k=6)
        wgt = 1.0 / np.maximum(d, 1e-4) ** 2
        ref = (refs[nn] * wgt[..., None]).sum(1) / wgt.sum(1)[:, None]
        red = chroma[:, 0] - chroma[:, 1]
        excess = red - (ref[:, 0] - ref[:, 1])
        w = ss(0.01, 0.04, excess) * wr[skin]
        # pulled: the chroma all the way to the skin round it, the luminance up (never down) by at most 10 %
        dl = np.clip(ref[:, 3] - ll[:, 0], 0.0, 0.10)
        new_log = logc + (ref[:, :3] - chroma) * w[:, None] + (dl * w)[:, None]
        # 2. edges: the low band's chroma may depart from a smooth 3D field of itself by 1.5 % only, so a painted
        # zone's hard colour border becomes a gradient over a few centimetres (luminance is left alone: its gradients
        # are shading, e.g. where the skin fades into the painted sideburns). The field leaves the dark painted
        # sideburns / hairline out (their brown would bleed into the skin).
        cur = new_log
        cll = cur.mean(-1, keepdims=True); cch = cur - cll
        lum_rel = np.exp(cll[:, 0]) / np.exp(np.median(cll))
        fw = ss(0.6, 0.85, lum_rel)
        feat2 = np.concatenate([cch, cll], -1)
        refs2 = np.empty((len(smp), 4))
        for j, i in enumerate(smp):
            n = np.array(tsrc.query_ball_point(pts[i], 2.4 * SMOOTH_S), np.int64)
            if len(n) < 8: refs2[j] = feat2[i]; continue
            g = np.exp(-np.sum((pts[src[n]] - pts[i]) ** 2, -1) / (2 * SMOOTH_S * SMOOTH_S)) * fw[src[n]]
            refs2[j] = (feat2[src[n]] * g[:, None]).sum(0) / max(g.sum(), 1e-9)
        sm = (refs2[nn] * wgt[..., None]).sum(1) / wgt.sum(1)[:, None]
        dch = np.clip(cch - sm[:, :3], -EDGE_C, EDGE_C)
        w2 = wr[skin] * fw
        smoothed = sm[:, :3] + dch + cll                       # chroma only: luminance gradients are shading, kept
        new_log = cur + (smoothed - cur) * w2[:, None]
        edge = np.abs((cch - sm[:, :3])[:, 0] - (cch - sm[:, :3])[:, 1])
        if verbose: print(f"  low band redness off its smooth field: p95 {np.percentile(edge[w2 > 0.5], 95) * 100:.1f} %, "
              f"p99 {np.percentile(edge[w2 > 0.5], 99) * 100:.1f} %")
        if verbose: print(f"{os.path.basename(f)}: region texels {len(pts)}, redness above the skin round it: p50 "
              f"{np.median(excess) * 100:.1f} %, p99 {np.percentile(excess, 99) * 100:.1f} %, max {excess.max() * 100:.1f} %; "
              f"texels pulled {int((w > 0.05).sum())}")
        if not verbose: pass
        corr = np.zeros_like(small)
        corr[skin] = new_log - logc
        # smooth the correction a little inside each island (no texel-scale steps)
        for k in np.unique(isl[skin]):
            m = (isl == k).astype(np.float64)
            ck = gauss(corr * m[..., None], 2.0) / np.maximum(gauss(m, 2.0), 1e-6)[..., None]
            corr[isl == k] = ck[isl == k]
        return np.where(cov[..., None], corr, 0.0)


if __name__ == "__main__":
    main()
