# Straight colour steps inside a UV island (painted "box" edges) blended into smooth ramps.
#   python3 tools/model/native/uv_lines.py <albedo.jpg> [...] --glb <role>.glb --urange 0.855,0.955 [--report]
# The MakeHuman male skins under Marcus' paint carry a beard zone with hard straight side edges: inside the head UV
# island two lines of constant v (v ~0.341 and ~0.692, symmetric about the mouth) where the colour steps by 6-10 %.
# On the face they run from the cheek down behind the jaw: a hard tone edge between the lower face and the neck in
# every close-up. Lines are found from the data (rows of constant v whose step across, inside the island and away from
# its border, is far above the island's median gradient over the given u range); for each one the step is measured
# column by column (log RGB, low-passed) and split between its two sides as linear ramps that reach the line from
# RAMP texels away, so both sides meet at the line's mid colour. Detail above the low-pass stays as painted.
import argparse, os, sys
import numpy as np
from PIL import Image
from scipy import ndimage
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "male"))
from rim_repair import raster_islands
from neck_tone import load_glb

R4 = 4096
RAMP = 110          # texels each side over which a line's step is spread
GAP = 4             # texels each side of the line not measured (the line itself)
WIDTH = 14          # texels measured each side


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("albedo", nargs="+")
    ap.add_argument("--glb", required=True)
    ap.add_argument("--urange", default="0.855,0.955")
    ap.add_argument("--ymin", type=float, default=1.40)
    ap.add_argument("--factor", type=float, default=4.0, help="a line's step must be this many times the median gradient")
    ap.add_argument("--iters", type=int, default=4)
    ap.add_argument("--report", action="store_true")
    a = ap.parse_args()
    ua, ub = (int(float(v) * R4) for v in a.urange.split(","))
    pos, uv, tri = load_glb(a.glb)
    isl = raster_islands(pos, uv, tri, a.ymin)
    ids, cnt = np.unique(isl[isl >= 0], return_counts=True)
    M = isl == ids[np.argmax(cnt)]                                    # the head island
    inner = ndimage.distance_transform_edt(M) > 24
    for f in a.albedo:
        im0 = np.asarray(Image.open(f).convert("RGB")).astype(np.float32)
        total = np.zeros_like(im0)
        for it in range(1 if a.report else a.iters):
            im = im0 * np.exp(total)
            corr = line_pass(im, inner, M, ua, ub, a, f if it == 0 else None)
            if corr is None: break
            total += corr
        if a.report or not np.any(total): continue
        out = np.clip(im0 * np.exp(total) + 0.5, 0, 255).astype(np.uint8)
        Image.fromarray(out).save(f, quality=88, optimize=True)
        line_pass(out.astype(np.float32), inner, M, ua, ub, a, f + " (after)")
        print(f"  corrected texels {int((np.abs(total).max(-1) > 0.005).sum())}, largest change {np.abs(total).max() * 100:.0f} %")


def line_pass(im, inner, M, ua, ub, a, label):
    """the log-correction of one pass over every line found (None when there is none)"""
    if True:   # (one block: the same names as the measuring code above)
        L = np.log(np.maximum(im, 1.0))
        w = inner.astype(np.float32)
        Ls = ndimage.gaussian_filter(L * w[..., None], (5, 5, 0)) / np.maximum(ndimage.gaussian_filter(w, 5), 1e-6)[..., None]
        both = inner[1:] & inner[:-1]
        g = np.abs(np.diff(Ls, axis=0)).sum(-1) * both
        cnt_ = both[:, ua:ub].sum(1)
        prof = g[:, ua:ub].sum(1) / np.maximum(cnt_, 1)
        med = np.median(prof[cnt_ > 60])
        strong = (prof > a.factor * med) & (cnt_ > 60)
        # lines: runs of strong rows, each reduced to its strongest row
        lines = []
        r = 0
        while r < len(prof):
            if strong[r]:
                s = r
                while r < len(prof) and strong[r]: r += 1
                lines.append(s + int(np.argmax(prof[s:r])))
            r += 1
        skin_lum = float(np.median(np.exp(Ls[inner].mean(-1))))
        skin = np.exp(Ls.mean(-1)) > 0.6 * skin_lum                    # the correction never touches painted hair
        corr = np.zeros_like(L)
        for vl in lines:
            lo = Ls[vl - GAP - WIDTH: vl - GAP].mean(0)                   # per column u: the side below
            hi = Ls[vl + GAP + 1: vl + GAP + 1 + WIDTH].mean(0)
            ok = inner[vl - GAP - WIDTH: vl - GAP].all(0) & inner[vl + GAP + 1: vl + GAP + 1 + WIDTH].all(0)
            ok[:ua] = False; ok[ub:] = False
            # skin on both sides only (the line runs on as the front edge of the painted sideburns: never blend those)
            ok &= (np.exp(lo.mean(-1)) > 0.7 * skin_lum) & (np.exp(hi.mean(-1)) > 0.7 * skin_lum)
            ok &= np.abs(hi - lo).sum(-1) < 0.30
            step = np.where(ok[:, None], hi - lo, 0.0)
            # smooth along the line, fading at the ends of the measured stretch
            wcol = ndimage.gaussian_filter(ok.astype(np.float32), 10)
            step = ndimage.gaussian_filter(step, (10, 0)) / np.maximum(wcol, 1e-6)[:, None] * np.clip(wcol * 2.0, 0, 1)[:, None]
            span = np.nonzero(ok)[0]
            if len(span) == 0: continue
            stats = np.abs(step[span]).sum(-1)
            if label: print(f"{os.path.basename(label)}: line at v={vl / R4:.4f}, u {span.min() / R4:.3f}..{span.max() / R4:.3f}, "
                  f"step median {np.median(stats) * 100:.1f} %, max {stats.max() * 100:.1f} % (log RGB sum)")
            for dv in range(-RAMP, RAMP + 1):
                row = vl + dv
                if row < 0 or row >= R4: continue
                t = 1.0 - abs(dv) / RAMP
                sgn = 0.5 if dv <= 0 else -0.5                            # below: up by half the step, above: down
                corr[row] += sgn * step * t
        if not lines: return None
        return corr * (M & skin)[..., None]


if __name__ == "__main__":
    main()
