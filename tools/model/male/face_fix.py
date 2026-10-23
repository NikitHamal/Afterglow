# Marcus' lower face: the painted albedos carry a red-pink "beard shadow" from the base of the nose to the chin with a
# hard edge across the face; under warm light it reads as a red smear. This moves that region's mean colour to his
# cheek tone (slightly darker and cooler, like stubble shadow), keeps the fine grain (stubble), feathers the edge and
# leaves the lips alone. Operates on the shipped albedos in place (idempotent enough to re-run: it measures first).
#   python3 tools/model/male/face_fix.py native/assets/characters
# UV layout (from marcus.glb): the face island spans u 0.722-0.961, v 0.415-0.619; up on the face is -u, the
# nose base sits at u ~0.884, the mouth at u ~0.910, v ~0.515.
import sys
import numpy as np
from PIL import Image

U0, U1, V0, V1 = 0.70, 0.985, 0.40, 0.64       # work box (a margin around the face island)
NOSE_BASE, FEATHER = 0.884, 0.010              # the painted edge, softened over +-FEATHER
LIPS, LIP_R = (0.9076, 0.517), (0.0105, 0.0235)   # mouth centre / radii (u, v), kept as painted


def blur(a, r):
    """separable gaussian (sigma r px), edges clamped"""
    k = np.exp(-0.5 * (np.arange(-3 * r, 3 * r + 1) / r) ** 2); k /= k.sum()
    p = len(k) // 2
    a = np.pad(a.astype(np.float32), p, mode="edge")
    a = np.apply_along_axis(lambda x: np.convolve(x, k, mode="valid"), 0, a)
    return np.apply_along_axis(lambda x: np.convolve(x, k, mode="valid"), 1, a)


def fix(path):
    im = Image.open(path).convert("RGB")
    W, H = im.size
    x0, x1, y0, y1 = int(U0 * W), int(U1 * W), int(V0 * H), int(V1 * H)
    box = np.asarray(im.crop((x0, y0, x1, y1)), np.float32) / 255.0
    Y = 0.299 * box[..., 0] + 0.587 * box[..., 1] + 0.114 * box[..., 2]
    Cb, Cr = box[..., 2] - Y, box[..., 0] - Y
    u = (x0 + np.arange(x1 - x0)[None, :] + 0.5) / W
    v = (y0 + np.arange(y1 - y0)[:, None] + 0.5) / H
    skin = Y > 0.18                                                     # the island, not its dark padding
    # the reference: cheeks / forehead above the edge, away from eyes and the island's rim
    ref = skin & (u > 0.80) & (u < NOSE_BASE - 0.02) & (np.abs(v - 0.517) > 0.035) & (np.abs(v - 0.517) < 0.08)
    yr, cbr, crr = np.median(Y[ref]), np.median(Cb[ref]), np.median(Cr[ref])
    low = skin & (u > NOSE_BASE + FEATHER) & (u < 0.955) & ~(((u - LIPS[0]) / LIP_R[0]) ** 2 + ((v - LIPS[1]) / LIP_R[1]) ** 2 < 1.5)
    excess = np.median(Cr[low]) - crr if low.any() else 0.0
    if excess < 0.008:
        print(f"{path}: lower face already matches the cheeks (Cr excess {excess:+.3f}), left as is")
        return
    m = np.clip((u - (NOSE_BASE - FEATHER)) / (2 * FEATHER), 0, 1)
    m = m * m * (3 - 2 * m) * np.clip((0.962 - u) / 0.01, 0, 1)           # fade out at the chin's rim too
    lip = ((u - LIPS[0]) / LIP_R[0]) ** 2 + ((v - LIPS[1]) / LIP_R[1]) ** 2
    m = m * np.clip((lip - 1.0) / 0.8, 0, 1) * skin
    m = blur(m, 6)
    # mean colour toward the cheeks (a touch darker, cooler: stubble shadow), fine grain kept
    Yb, Cbb, Crb = blur(Y, 10), blur(Cb, 10), blur(Cr, 10)
    Y2 = Y + m * (yr * 0.93 - Yb) * 0.7
    Cb2 = Cb + m * (cbr + 0.004 - Cbb)
    Cr2 = Cr + m * (crr - 0.006 - Crb)
    R, B = Cr2 + Y2, Cb2 + Y2
    G = (Y2 - 0.299 * R - 0.114 * B) / 0.587
    out = np.clip(np.dstack([R, G, B]), 0, 1)
    full = np.asarray(im, np.uint8).copy()
    full[y0:y1, x0:x1] = (out * 255 + 0.5).astype(np.uint8)
    Image.fromarray(full).save(path, quality=88, optimize=True)   # about the original encoding (size stays put)
    print(f"{path}: lower face Cr excess {excess:+.3f} -> moved to the cheek tone ({int(m.sum())} px weighted)")


if __name__ == "__main__":
    d = sys.argv[1] if len(sys.argv) > 1 else "native/assets/characters"
    for tone in ("honey", "caramel", "deep"):
        fix(f"{d}/marcus_skin_{tone}.jpg")
