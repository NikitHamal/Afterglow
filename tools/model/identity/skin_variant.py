# A model's own skin albedos from its base's painted tones (no hand edits): lips recoloured, a little blush, a man's
# stubble — painted in the shared UV layout, so every tone of the base stays matched (same UVs, normal and ORM maps).
#   python3 tools/model/identity/skin_variant.py <model id> <base.glb> <characters dir> [tones...]
# Regions come from the GLB itself: the mouth from the lip morphs (kiss + pout + smile) splatted into UV space, the lips
# from the redness the base already has painted there (crisp edges), the beard from head-frame geometry.
import json, struct, sys, os
import numpy as np
from PIL import Image, ImageFilter

RECIPES = {
    # lips: sRGB target, strength; blush: sRGB, strength; stubble: strength
    "mei":    {"base": "vivian", "tones": ["fair", "porcelain", "honey"], "lips": ((0.70, 0.38, 0.40), 0.78), "blush": ((0.86, 0.55, 0.55), 0.10)},
    "kari":   {"base": "vivian", "tones": ["honey", "olive", "caramel"], "lips": ((0.62, 0.16, 0.18), 0.85), "blush": ((0.82, 0.52, 0.50), 0.08)},
    "amara":  {"base": "vivian", "tones": ["deep", "ebony", "bronze"], "lips": ((0.27, 0.10, 0.12), 0.6), "blush": None},
    # stubble (beard field below) is supported but not shipped: painted grain read as a rash in close-ups
    "daniel": {"base": "marcus", "tones": [], "lips": None, "blush": None, "stubble": None},
}

def read_glb(path):
    b = open(path, "rb").read(); off, js, bn = 12, None, None
    while off < len(b):
        cl, ct = struct.unpack_from("<II", b, off); off += 8
        if ct == 0x4E4F534A: js = json.loads(b[off:off + cl])
        elif ct == 0x004E4942: bn = b[off:off + cl]
        off += cl
    return js, bn
def acc(js, bn, i):
    a = js["accessors"][i]; n = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}[a["type"]]
    dt = np.dtype({5126: np.float32, 5123: np.uint16, 5121: np.uint8, 5125: np.uint32}[a["componentType"]])
    out = np.zeros((a["count"], n), dt)
    if "bufferView" in a:
        bv = js["bufferViews"][a["bufferView"]]; st = bv.get("byteStride", 0) or dt.itemsize * n
        o = bv.get("byteOffset", 0) + a.get("byteOffset", 0)
        raw = np.frombuffer(bn, np.uint8, st * (a["count"] - 1) + dt.itemsize * n, o)
        idx = np.arange(a["count"])[:, None] * st + np.arange(dt.itemsize * n)[None]
        out = raw[idx].copy().view(dt).reshape(-1, n)
    if "sparse" in a:
        sp = a["sparse"]; ib, vb = js["bufferViews"][sp["indices"]["bufferView"]], js["bufferViews"][sp["values"]["bufferView"]]
        idt = np.dtype({5123: np.uint16, 5125: np.uint32, 5121: np.uint8}[sp["indices"]["componentType"]])
        ii = np.frombuffer(bn, idt, sp["count"], ib.get("byteOffset", 0) + sp["indices"].get("byteOffset", 0))
        vv = np.frombuffer(bn, dt, sp["count"] * n, vb.get("byteOffset", 0) + sp["values"].get("byteOffset", 0)).reshape(-1, n)
        out = out.copy(); out[ii.astype(np.int64)] = vv
    return out.astype(np.float64)
def lin(c): return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)
def srgb(c): c = np.clip(c, 0, 1); return np.where(c <= 0.0031308, c * 12.92, 1.055 * c ** (1 / 2.4) - 0.055)
def ss(a, b, x): t = np.clip((x - a) / (b - a), 0, 1); return t * t * (3 - 2 * t)

def blur(a, sigma):
    """separable Gaussian blur of a float image (edges clamped)"""
    r = max(1, int(3 * sigma)); x = np.arange(-r, r + 1); k = np.exp(-0.5 * (x / sigma) ** 2); k /= k.sum()
    a = np.pad(a, ((r, r), (0, 0)), mode="edge"); a = sum(k[i] * a[i:i + a.shape[0] - 2 * r] for i in range(2 * r + 1))
    a = np.pad(a, ((0, 0), (r, r)), mode="edge"); return sum(k[i] * a[:, i:i + a.shape[1] - 2 * r] for i in range(2 * r + 1))

def splat(uv, val, R=1024, sigma=2.0):
    """per-vertex values -> a smooth UV field (glTF UV: v down = image rows)"""
    acc_v = np.zeros((R, R)); acc_w = np.zeros((R, R))
    px = np.clip((uv[:, 0] * R).astype(int), 0, R - 1); py = np.clip((uv[:, 1] * R).astype(int), 0, R - 1)
    np.add.at(acc_v, (py, px), val); np.add.at(acc_w, (py, px), 1.0)
    return blur(acc_v, sigma) / np.maximum(blur(acc_w, sigma), 1e-6)

def main():
    mid, glb, chars = sys.argv[1], sys.argv[2], sys.argv[3]
    rc = RECIPES[mid]; tones = sys.argv[4:] or rc["tones"]
    js, bn = read_glb(glb)
    body = next(m for m in js["meshes"] if m["name"].endswith("_body"))
    prim = body["primitives"][0]; names = body.get("extras", {}).get("targetNames", [])
    P = acc(js, bn, prim["attributes"]["POSITION"]); UV = acc(js, bn, prim["attributes"]["TEXCOORD_0"])
    def target(n):
        return acc(js, bn, prim["targets"][names.index(n)]["POSITION"]) if n in names else np.zeros_like(P)
    mouthv = sum(np.linalg.norm(target(n), axis=1) for n in ("kiss", "pout", "smile", "moan") if n in names)
    mouthv = mouthv / max(mouthv.max(), 1e-9)
    # head frame (glTF: y up, +z forward)
    head = P[P[:, 1] > P[:, 1].max() - 0.26]
    top = P[:, 1].max(); front = head[:, 2].max()
    R = 1024
    mouth = splat(UV, ss(0.25, 0.6, mouthv), R, 2.5)
    mouth = np.clip(mouth * 1.4, 0, 1)
    beard = None
    if rc.get("stubble"):
        # upper lip, chin, jaw and the cheeks below the cheekbones; never the nose. Measured from the mouth itself.
        y = P[:, 1]; z = P[:, 2]; x = P[:, 0]
        mc = P[mouthv > 0.8].mean(0)
        nose_base = mc[1] + 0.020; chin_y = mc[1] - 0.050
        w = ss(nose_base + 0.004, nose_base - 0.006, y) * ss(chin_y - 0.035, chin_y - 0.010, y)
        w *= ss(mc[2] - 0.115, mc[2] - 0.085, z) * ss(0.080, 0.064, np.abs(x)) * (1 - ss(0.45, 0.8, mouthv))
        w *= np.where(np.abs(x) > 0.022, ss(nose_base - 0.004, nose_base - 0.02, y), 1.0)
        beard = splat(UV, w, R, 2.0)
    lipmask = None
    if rc.get("lips"):
        # one lip mask for every tone (shared UVs): from the lightest painted tone, where the lips stand out most
        for ref_tone in ("porcelain", "fair", *tones):
            ref = os.path.join(chars, f"{rc['base']}_skin_{ref_tone}.jpg")
            if os.path.exists(ref): break
        im = np.asarray(Image.open(ref).convert("RGB"), np.float64) / 255.0
        Hh, Ww = im.shape[:2]
        M = np.asarray(Image.fromarray(mouth.astype(np.float32)).resize((Ww, Hh), Image.BICUBIC), np.float64)
        red = (im[..., 0] - 0.5 * (im[..., 1] + im[..., 2]))
        core = M > 0.35
        hist, edges = np.histogram(red[core], 128); cen = 0.5 * (edges[1:] + edges[:-1])
        wsum = np.cumsum(hist); msum = np.cumsum(hist * cen); tot = wsum[-1]
        between = (msum[-1] * wsum / tot - msum) ** 2 / np.maximum(wsum * (tot - wsum), 1)
        thr = cen[int(np.argmax(between))]
        lipmask = blur(core * ss(thr - 0.015, thr + 0.025, red), 1.0) * ss(0.25, 0.5, M)
        print(f"{mid}: lip mask from {os.path.basename(ref)}: {int((lipmask > 0.5).sum())} px")
    for tone in tones:
        src = os.path.join(chars, f"{rc['base']}_skin_{tone}.jpg")
        if not os.path.exists(src): print("skip", src); continue
        im = np.asarray(Image.open(src).convert("RGB"), np.float64) / 255.0
        H, W = im.shape[:2]
        up = lambda f: np.asarray(Image.fromarray(f.astype(np.float32)).resize((W, H), Image.BICUBIC), np.float64)
        L = lin(im)
        if lipmask is not None:
            lipm = lipmask if lipmask.shape == L.shape[:2] else np.asarray(Image.fromarray(lipmask.astype(np.float32)).resize((W, H), Image.BICUBIC), np.float64)
            tgt, k = rc["lips"]; tl = lin(np.array(tgt))
            lum = L @ np.array([0.2126, 0.7152, 0.0722]); lm = np.median(lum[lipm > 0.5]) if (lipm > 0.5).any() else 0.1
            recol = tl[None, None] * np.clip(lum / max(lm, 1e-4), 0.4, 1.8)[..., None]
            L = L * (1 - (lipm * k)[..., None]) + recol * (lipm * k)[..., None]
            print(f"{mid} {tone}: lips {int((lipm > 0.5).sum())} px")
        if rc.get("blush"):
            # cheeks: the mouth field's broad halo minus the mouth itself, a soft warm wash
            cheeks = np.clip(up(splat(UV, ss(0.05, 0.25, mouthv), R, 14.0)) * 1.6 - up(mouth), 0, 1)
            tgt, k = rc["blush"]
            L = L * (1 - (cheeks * k)[..., None]) + (L * lin(np.array(tgt)) / max(lin(np.array(tgt)).max(), 1e-3))[...] * (cheeks * k)[..., None]
        if beard is not None:
            B = up(beard)
            rng = np.random.default_rng(7)
            dots = blur((rng.random((H, W)) < 0.24).astype(np.float64), 0.5)
            shade = 1.0 - rc["stubble"] * B * (0.25 + 0.75 * np.clip(dots * 3.0, 0, 1))
            L = L * shade[..., None] * (np.array([0.86, 0.88, 0.93])[None, None] ** B[..., None])
            print(f"{mid} {tone}: stubble over {int((B > 0.3).sum())} px")
        out = os.path.join(chars, f"{mid}_skin_{tone}.jpg")
        Image.fromarray((srgb(L) * 255 + 0.5).astype(np.uint8)).save(out, quality=88, optimize=True)
        print("wrote", out, os.path.getsize(out) // 1024, "KB")

if __name__ == "__main__":
    main()
