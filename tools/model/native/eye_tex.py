# Eye albedo for the native game: a clearer, more alive iris on the MakeHuman eye texture (no hand edits).
#   python3 tools/model/native/eye_tex.py <character.glb> <out.png> [--sat 1.25] [--gain 1] [--limbus 0.6] [--sclera 0.9]
#   (shipped: vivian defaults; marcus --sat 0.9 --gain 0.8 — his brown is already warm, more saturation turns it red)
# Reads the GLB's embedded `*_eye` image, finds both iris discs (dark, round, one per eyeball) and, in linear light:
#   - deepens the limbal ring (the dark rim where the iris meets the white, the strongest "alive" cue),
#   - lifts iris saturation and darkens it gently toward the limbus (it sits under the cornea, never washed out),
#   - makes the pupil a clean near-black disc,
#   - tones the sclera down to a warm off-white (a white sclera blooms and reads as a blank stare).
# The game loads the result as `characters/<id>_eye.png`, replacing the embedded albedo (alpha kept as is).
import json, struct, sys, io
import numpy as np
from PIL import Image


def glb_image(path, key):
    b = open(path, "rb").read()
    off, js, bn = 12, None, None
    while off < len(b):
        cl, ct = struct.unpack_from("<II", b, off); off += 8
        if ct == 0x4E4F534A: js = json.loads(b[off:off + cl])
        elif ct == 0x004E4942: bn = b[off:off + cl]
        off += cl
    for im in js["images"]:
        if key in im.get("name", ""):
            bv = js["bufferViews"][im["bufferView"]]
            o = bv.get("byteOffset", 0)
            return Image.open(io.BytesIO(bn[o:o + bv["byteLength"]])).convert("RGBA")
    raise SystemExit(f"no image named *{key}* in {path}")


def to_lin(c): return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)
def to_srgb(c): c = np.clip(c, 0, 1); return np.where(c <= 0.0031308, c * 12.92, 1.055 * c ** (1 / 2.4) - 0.055)
def sstep(a, b, x): t = np.clip((x - a) / (b - a), 0, 1); return t * t * (3 - 2 * t)


def iris_discs(rgb, alpha):
    """Both irises (one on each side of the texture's diagonal, MakeHuman layout): the centre is the pupil's centroid,
    the radius is where the radial brightness profile climbs from the iris to the white."""
    h, w = alpha.shape
    lum = rgb @ np.array([0.299, 0.587, 0.114])
    yy, xx = np.mgrid[0:h, 0:w]
    out = []
    for side in (xx < yy, xx > yy):
        dark = (lum < 0.42) & (alpha > 0.5) & side
        cx, cy = xx[dark].mean(), yy[dark].mean()
        for _ in range(4):                          # rough: the compact dark core of this eyeball
            core = dark & (np.hypot(xx - cx, yy - cy) < 0.2 * w)
            cx, cy = xx[core].mean(), yy[core].mean()
        pm = (lum < 0.05) & (np.hypot(xx - cx, yy - cy) < 0.12 * w)
        cx, cy = xx[pm].mean(), yy[pm].mean()      # exact: the pupil
        pr = np.sqrt(pm.sum() / np.pi)
        r = np.hypot(xx - cx, yy - cy)
        # the iris edge: where the radial brightness profile, going out, crosses half-way from the iris to the white
        ks = range(int(0.3 * w))
        prof = np.array([lum[(r >= k) & (r < k + 1) & (alpha > 0.5)].mean() for k in ks])
        irisL = np.median(prof[int(1.5 * pr):int(3.0 * pr)])
        whiteL = np.median(prof[int(0.16 * w):int(0.2 * w)])
        k0 = int(2.0 * pr)
        k1 = int(np.argmax(prof[k0:] > 0.5 * (irisL + whiteL))) + k0
        R = float(k1)
        out.append((cx, cy, R, pr))
    return out


def main():
    args = sys.argv[1:]
    glb, out = args[0], args[1]
    opt = {"--sat": 1.25, "--gain": 1.0, "--limbus": 0.6, "--sclera": 0.9}
    for k in opt:
        if k in args: opt[k] = float(args[args.index(k) + 1])
    img = glb_image(glb, "_eye")
    a = np.asarray(img).astype(np.float32) / 255.0
    srgb, alpha = a[..., :3], a[..., 3]
    lin = to_lin(srgb)
    h, w = alpha.shape
    yy, xx = np.mgrid[0:h, 0:w]
    discs = iris_discs(srgb, alpha)
    lum = lin @ np.array([0.2126, 0.7152, 0.0722])
    res = lin.copy()
    irisAll = np.zeros_like(alpha)
    for cx, cy, R, pr in discs:
        r = np.sqrt((xx - cx) ** 2 + (yy - cy) ** 2) / R
        iris = 1.0 - sstep(0.97, 1.06, r)
        irisAll = np.maximum(irisAll, iris)
        # richer colour, gently darker toward the rim (under the cornea's edge)
        l = lum[..., None]
        sat = l + (res - l) * opt["--sat"]
        sat *= (opt["--gain"] * (1.0 - 0.28 * sstep(0.62, 1.0, r)))[..., None]
        res = res * (1 - iris[..., None]) + np.clip(sat, 0, 1) * iris[..., None]
        # limbal ring: a soft dark band straddling the iris edge
        ring = np.exp(-((r - 0.93) / 0.055) ** 2)
        res *= (1.0 - opt["--limbus"] * ring)[..., None]
        # pupil: the texture's own dark disc, made clean near-black (same size)
        pp = (1.0 - sstep(pr / R * 0.85, pr / R * 1.05, r)) * (1.0 - sstep(0.02, 0.06, lum))
        res = res * (1 - pp[..., None]) + np.array([0.004, 0.003, 0.003]) * pp[..., None]
        print(f"iris at ({cx:.0f}, {cy:.0f}) r={R:.0f}px pupil={pr:.0f}px")
    # sclera: warm off-white, never brighter than ~0.55 linear
    sclera = (1.0 - irisAll) * (alpha > 0.5)
    tone = res * opt["--sclera"] * np.array([1.0, 0.975, 0.94])
    over = np.maximum(tone.max(-1, keepdims=True) / 0.55, 1.0)
    tone = tone / over
    res = res * (1 - sclera[..., None]) + tone * sclera[..., None]
    o = np.concatenate([to_srgb(res), alpha[..., None]], -1)
    Image.fromarray((o * 255 + 0.5).astype(np.uint8), "RGBA").save(out, optimize=True)
    print("wrote", out)


if __name__ == "__main__":
    main()
