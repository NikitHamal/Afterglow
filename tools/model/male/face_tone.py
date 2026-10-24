# Marcus' face, at the source of the shipped albedos: no beard box, no red mouth.
#   python3 tools/model/male/face_tone.py native/assets/characters [marcus.glb]   (all three painted tones, in place)
# The MakeHuman base skins under paint.py carry their own beard zone: everything below the nose base is a flat box,
# darker than the cheeks and edged across the whole face (face_fix.py moved its colour but the box stayed), and the
# philtrum, the skin round the lips and the lips themselves run red. In warm light that reads as a red, painted-on
# lower face. Frequency separation: only the large-scale tone (> ~40 px at 4K) of the lower face is replaced, by the
# cheeks' tone carried down across the box (normalized convolution) with a soft stubble shade instead of an edge; everything finer (pores, stubble dots, the mouth's shape) stays as painted, the lips keep part of their
# colour; the jaw/neck wings lose the source's red blotches; the upper face and the under-chin are untouched.
# The head's UV island is found from marcus.glb's own triangles (the UV padding is skin-coloured: colour can't tell).
# UV layout: face up = -u; nose base u ~0.884, mouth centre (0.9076, 0.517).
import json, struct, sys
from collections import defaultdict
import numpy as np
from PIL import Image

NOSE_BASE = 0.884
LIPS, LIP_R = (0.9076, 0.517), (0.0105, 0.0235)
SEED = (0.86, 0.47)                                  # a cheek: the head island grows from here


def glb_tris(glb):
    """UV triangles of the body mesh (first primitive of the first mesh); interleaved buffer views honoured"""
    b = open(glb, "rb").read()
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
    uv = acc(pr["attributes"]["TEXCOORD_0"]).astype(np.float64)
    return uv[acc(pr["indices"]).reshape(-1, 3).astype(np.int64)]


def head_island(tris):
    """triangles connected (through shared UV corners) to the one under SEED"""
    key = lambda p: (round(p[0] * 20000), round(p[1] * 20000))
    v2t = defaultdict(list)
    for i, t in enumerate(tris):
        for p in t: v2t[key(p)].append(i)
    c = tris.mean(1)
    seed = int(np.argmin(np.hypot(c[:, 0] - SEED[0], c[:, 1] - SEED[1])))
    seen, st = {seed}, [seed]
    while st:
        for p in tris[st.pop()]:
            for j in v2t[key(p)]:
                if j not in seen: seen.add(j); st.append(j)
    return tris[sorted(seen)]


def raster(tris, x0, y0, w, h, W, H):
    """coverage (0/1) of UV triangles on the texture's pixel grid inside the box (x0, y0, w, h)"""
    m = np.zeros((h, w), np.float32)
    P = tris * np.array([W, H]) - np.array([x0, y0])
    for t in P:
        lo = np.maximum(np.floor(t.min(0)).astype(int), 0); hi = np.minimum(np.ceil(t.max(0)).astype(int) + 1, [w, h])
        if (hi <= lo).any(): continue
        X, Yy = np.meshgrid(np.arange(lo[0], hi[0]) + 0.5, np.arange(lo[1], hi[1]) + 0.5)
        (ax, ay), (bx, by), (cx, cy) = t
        d = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
        if abs(d) < 1e-12: continue
        l1 = ((by - cy) * (X - cx) + (cx - bx) * (Yy - cy)) / d
        l2 = ((cy - ay) * (X - cx) + (ax - cx) * (Yy - cy)) / d
        ins = (l1 >= -0.02) & (l2 >= -0.02) & (1 - l1 - l2 >= -0.02)
        m[lo[1]:hi[1], lo[0]:hi[0]] = np.maximum(m[lo[1]:hi[1], lo[0]:hi[0]], ins)
    return m


def blur(a, r):
    """separable gaussian (sigma r px), edges clamped"""
    k = np.exp(-0.5 * (np.arange(-3 * r, 3 * r + 1) / r) ** 2); k /= k.sum()
    p = len(k) // 2
    a = np.pad(a.astype(np.float32), p, mode="edge")
    a = np.apply_along_axis(lambda x: np.convolve(x, k, mode="valid"), 0, a)
    return np.apply_along_axis(lambda x: np.convolve(x, k, mode="valid"), 1, a)


def down(a, f): return np.asarray(Image.fromarray(a.astype(np.float32), "F").resize((a.shape[1] // f, a.shape[0] // f), Image.BOX))
def up(a, shape): return np.asarray(Image.fromarray(a.astype(np.float32), "F").resize((shape[1], shape[0]), Image.BILINEAR))


def sstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0, 1); return t * t * (3 - 2 * t)


def fix(path, scale, isl):
    im = Image.open(path).convert("RGB")
    W, H = im.size
    lo, hi = isl.reshape(-1, 2).min(0), isl.reshape(-1, 2).max(0)
    x0, y0 = max(0, int(lo[0] * W) - 8), max(0, int(lo[1] * H) - 8)
    x1, y1 = min(W, int(hi[0] * W) + 8), min(H, int(hi[1] * H) + 8)
    box = np.asarray(im.crop((x0, y0, x1, y1)), np.float32) / 255.0
    Y = box @ np.array([0.299, 0.587, 0.114], np.float32)
    Cb, Cr = box[..., 2] - Y, box[..., 0] - Y
    u = (x0 + np.arange(x1 - x0)[None, :] + 0.5) / W
    v = (y0 + np.arange(y1 - y0)[:, None] + 0.5) / H
    face = raster(isl, x0, y0, x1 - x0, y1 - y0, W, H)
    skin = face * (Y > 0.18 * scale)                      # the island's skin, not its scalp paint
    side = np.abs(v - LIPS[1])
    lip = ((u - LIPS[0]) / LIP_R[0]) ** 2 + ((v - LIPS[1]) / LIP_R[1]) ** 2
    lips = 1.0 - sstep(0.8, 1.6, lip)
    # the lower face (box, jaw corners, round the mouth) takes its large-scale tone from the face above it,
    # extrapolated down (normalized convolution); the lips keep their own shading and part of their colour
    # the source: the cheeks only (below the eyes, off the nose and its shadow, above the box)
    trust = skin * sstep(0.79, 0.80, u) * (1.0 - sstep(NOSE_BASE - 0.012, NOSE_BASE - 0.004, u)) \
        * sstep(0.03, 0.04, side) * (1.0 - sstep(0.10, 0.12, side))
    # the face proper: not under the chin, not the jaw/neck wings (their seams to the neck stay as they were)
    lower = sstep(NOSE_BASE - 0.01, NOSE_BASE + 0.02, u) * (1.0 - sstep(0.94, 0.958, u)) * (1.0 - sstep(0.112, 0.135, side))
    rim = np.clip(blur(face, max(2, int(12 * W / 4096))) * 2.0 - 1.0, 0, 1) * skin   # the island's rim stays as is
    # the jaw/neck wings: the source's beard edge left darker, redder blotches down the jaw angle (a red band beside
    # the neck); their large-scale tone is rebuilt from the wing's own normal skin (up to the seam: it moves the
    # blotches toward the neck's tone, so the seam only gets quieter)
    wing = sstep(0.112, 0.135, side) * sstep(NOSE_BASE - 0.06, NOSE_BASE - 0.03, u) * (1.0 - sstep(0.955, 0.97, u))
    yr0, crr0 = float(np.median(Y[trust > 0.5])), float(np.median(Cr[trust > 0.5]))
    good = skin * (wing > 0.01) * (Cr < crr0 + 0.008) * (Y > 0.9 * yr0)
    f = 4                                                  # low-frequency fields on a 1/4 grid
    r_big, R = max(2, int(40 * W / 4096) // f), max(4, int(160 * W / 4096) // f)
    sk, tr, gd = down(skin, f), down(trust, f), down(good, f)
    wsk, wtr, wgd = blur(sk, r_big), blur(tr, R), blur(gd, R // 2)
    out = []
    for ch, name in ((Y, "Y"), (Cb, "Cb"), (Cr, "Cr")):
        c = down(ch, f)
        big = blur(c * sk, r_big) / np.maximum(wsk, 1e-6)
        fill = blur(c * tr, R) / np.maximum(wtr, 1e-6)
        med = float(np.median(ch[trust > 0.5]))
        fill = med + (fill - med) * sstep(0.005, 0.06, wtr)     # far from support: the cheeks' median, not noise
        d = up(fill - big, ch.shape)
        fw = blur(c * gd, R // 2) / np.maximum(wgd, 1e-6)
        fw = med + (fw - med) * sstep(0.005, 0.06, wgd)
        dw = up(fw - big, ch.shape)
        if name == "Y":
            k = lower * (1.0 - lips)
            d = d - 0.05 * (ch + d) * sstep(NOSE_BASE, NOSE_BASE + 0.03, u)   # a soft stubble shade, no edge
        else:
            k = lower * (1.0 - 0.55 * lips)                                    # lips: less red, still lips
            if name == "Cb": d = d + 0.003 * sstep(NOSE_BASE, NOSE_BASE + 0.03, u)   # stubble reads a touch cool
        if name == "Cb": dw = dw * 0.5
        out.append(ch + d * k * rim + dw * wing * skin * (1.0 - k))
    Y2, Cb2, Cr2 = out
    R_, B_ = Cr2 + Y2, Cb2 + Y2
    G_ = (Y2 - 0.299 * R_ - 0.114 * B_) / 0.587
    res = np.clip(np.dstack([R_, G_, B_]), 0, 1)
    full = np.asarray(im, np.uint8).copy()
    full[y0:y1, x0:x1] = (res * 255 + 0.5).astype(np.uint8)
    Image.fromarray(full).save(path, quality=88, optimize=True)
    low = (skin > 0) & (u > NOSE_BASE + 0.02) & (u < 0.94) & (lips < 0.1)
    ref = (skin > 0) & (u > 0.80) & (u < NOSE_BASE - 0.02) & (np.abs(v - 0.517) > 0.035) & (np.abs(v - 0.517) < 0.08)
    print(f"{path}: cheeks Y {np.median(Y[ref]):.3f} Cr {np.median(Cr[ref]):.3f} | lower face Y {np.median(Y[low]):.3f}"
          f" -> {np.median(Y2[low]):.3f}, Cr {np.median(Cr[low]):.3f} -> {np.median(Cr2[low]):.3f}")


if __name__ == "__main__":
    d = sys.argv[1] if len(sys.argv) > 1 else "native/assets/characters"
    isl = head_island(glb_tris(sys.argv[2] if len(sys.argv) > 2 else f"{d}/marcus.glb"))
    for tone, scale in (("honey", 1.0), ("caramel", 1.0), ("deep", 0.55)):
        fix(f"{d}/marcus_skin_{tone}.jpg", scale, isl)
