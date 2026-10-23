# Procedural room textures (numpy + PIL): wood planks, plaster/damask wall, velvet, satin, linen, rug, night sky.
# All tileable (except the sky); written as sRGB JPEG albedo + simple normal maps.
import numpy as np
from PIL import Image, ImageFilter
rng = np.random.default_rng(3)

def fbm(h, w, scale, octaves=5, seed=0):
    """tileable value-noise fbm in [0,1]"""
    r = np.random.default_rng(seed); out = np.zeros((h, w), np.float32); amp = 1.0; tot = 0.0
    for o in range(octaves):
        g = max(2, int(scale * 2 ** o))
        grid = r.random((g, g)).astype(np.float32)
        img = Image.fromarray((grid * 255).astype(np.uint8)).resize((w, h), Image.BICUBIC)
        # tile-safe: wrap by resizing a padded grid
        gp = np.pad(grid, ((0, 1), (0, 1)), mode="wrap")
        img = Image.fromarray((gp * 255).astype(np.uint8)).resize((int(w * (g + 1) / g), int(h * (g + 1) / g)), Image.BICUBIC)
        a = np.asarray(img, np.float32)[:h, :w] / 255.0
        out += a * amp; tot += amp; amp *= 0.5
    return out / tot

def save_rgb(a, path, q=90):
    Image.fromarray(np.clip(a * 255 + 0.5, 0, 255).astype(np.uint8)).save(path, quality=q)

def normal_from_height(h, k=2.0):
    dx = np.roll(h, -1, 1) - np.roll(h, 1, 1); dy = np.roll(h, -1, 0) - np.roll(h, 1, 0)
    n = np.dstack([-dx * k, dy * k, np.ones_like(h)]); n /= np.linalg.norm(n, axis=2, keepdims=True)
    return n * 0.5 + 0.5

def wood_planks(path, npath, S=1024):
    """warm walnut floor planks, 6 planks across, staggered ends"""
    y, x = np.mgrid[0:S, 0:S].astype(np.float32) / S
    planks = 6; pw = 1.0 / planks
    idx = np.floor(x / pw).astype(int)
    off = (idx * 0.37) % 1.0
    yy = (y + off) % 1.0
    seg = np.floor(yy * 2).astype(int)                      # two boards per plank column per tile
    tone = rng.uniform(0.82, 1.12, (planks, 2))[idx % planks, seg % 2]
    grain = fbm(S, S, 3, 5, 11)
    stretch = np.asarray(Image.fromarray((grain * 255).astype(np.uint8)).resize((S // 16, S)).resize((S, S), Image.BICUBIC), np.float32) / 255
    rings = 0.5 + 0.5 * np.sin((x * 90 + stretch * 7 + idx * 1.3) * 2.2)
    base = np.array([0.30, 0.17, 0.10])
    col = base[None, None] * (0.75 + 0.35 * stretch[..., None]) * (0.9 + 0.12 * rings[..., None]) * tone[..., None]
    gap = (np.abs((x % pw) - 0.0) < 0.0025) | (np.abs((x % pw) - pw) < 0.0025) | (np.abs((yy * 2) % 1.0) < 0.003)
    col[gap] *= 0.35
    save_rgb(col, path)
    h = 0.3 * stretch + 0.1 * rings - gap * 0.8
    save_rgb(normal_from_height(h, 3.0), npath)

def damask_wall(path, npath, S=1024):
    """deep warm plaster with a faint tone-on-tone damask motif"""
    y, x = np.mgrid[0:S, 0:S].astype(np.float32) / S
    n = fbm(S, S, 4, 5, 21)
    u, v = (x * 4) % 1.0 - 0.5, (y * 3) % 1.0 - 0.5
    motif = np.exp(-((np.abs(u) * 2.2) ** 2 + (v * 1.6) ** 2) * 6) * (0.5 + 0.5 * np.cos(np.arctan2(v, np.abs(u) + 1e-3) * 4))
    motif += 0.6 * np.exp(-((np.abs(u) - 0.32) ** 2 * 60 + (v ** 2) * 18))
    base = np.array([0.24, 0.10, 0.11])
    col = base[None, None] * (0.85 + 0.25 * n[..., None]) * (1.0 + 0.10 * motif[..., None])
    save_rgb(col, path)
    save_rgb(normal_from_height(n * 0.2 + motif * 0.15, 2.0), npath)

def fabric(path, npath, color, S=512, weave=96, sheen=0.25, seed=0):
    y, x = np.mgrid[0:S, 0:S].astype(np.float32) / S
    n = fbm(S, S, 6, 4, seed)
    w = 0.5 + 0.25 * np.sin(x * weave * 2 * np.pi) + 0.25 * np.sin(y * weave * 2 * np.pi)
    col = np.array(color)[None, None] * (0.86 + 0.18 * n[..., None]) * (1 - sheen * 0.2 + sheen * 0.2 * w[..., None])
    save_rgb(col, path)
    save_rgb(normal_from_height(w * 0.25 + n * 0.3, 1.5), npath)

def rug(path, S=1024):
    """muted persian-style rug: border bands + medallion field"""
    y, x = np.mgrid[0:S, 0:S].astype(np.float32) / S
    n = fbm(S, S, 8, 4, 31)
    d = np.minimum(np.minimum(x, 1 - x), np.minimum(y, 1 - y))
    field = np.array([0.32, 0.08, 0.08]); border = np.array([0.10, 0.07, 0.12]); gold = np.array([0.55, 0.40, 0.20])
    col = np.broadcast_to(field, (S, S, 3)).copy()
    col[d < 0.09] = border; col[(d > 0.07) & (d < 0.085)] = gold; col[(d > 0.03) & (d < 0.04)] = gold
    cx, cy = x - 0.5, y - 0.5; rr = np.sqrt((cx / 0.6) ** 2 + (cy / 0.8) ** 2)
    med = (rr < 0.22) & (d > 0.1)
    col[med] = border * 1.2; col[(np.abs(rr - 0.22) < 0.012) & (d > 0.1)] = gold
    pat = (np.sin(x * 60) * np.sin(y * 60) > 0.6) & (d > 0.1) & ~med
    col[pat] = col[pat] * 0.7 + gold * 0.3
    col *= (0.8 + 0.3 * n[..., None])
    save_rgb(col, path)

def night_sky(path, W=2048, H=1024):
    """city at night through the window: gradient sky, moon, stars, skyline with lit windows"""
    y, x = np.mgrid[0:H, 0:W].astype(np.float32); u, v = x / W, y / H
    sky = np.dstack([0.02 + 0.06 * v, 0.03 + 0.07 * v, 0.08 + 0.10 * v])
    stars = (rng.random((H, W)) > 0.9985) * rng.uniform(0.4, 1.0, (H, W)) * (v < 0.6)
    sky += stars[..., None] * np.array([0.9, 0.9, 1.0])
    mx, my, mr = 0.72, 0.22, 0.045
    d = np.sqrt(((u - mx) * W / H) ** 2 + (v - my) ** 2)
    sky += (np.exp(-(d / 0.18) ** 2) * 0.12)[..., None] * np.array([0.6, 0.7, 0.9])
    moon = d < mr
    sky[moon] = np.array([0.92, 0.93, 0.88]) * (0.85 + 0.15 * fbm(H, W, 30, 3, 5)[moon])[:, None]
    # skyline
    xs = 0; heights = []
    while xs < W:
        bw = int(rng.uniform(40, 140)); bh = rng.uniform(0.25, 0.62)
        heights.append((xs, xs + bw, bh)); xs += bw + int(rng.uniform(0, 12))
    for x0, x1, bh in heights:
        top = int(H * (1 - bh))
        sky[top:, x0:x1] = np.array([0.012, 0.012, 0.02])
        for wy in range(top + 8, H - 4, 14):
            for wx in range(x0 + 6, x1 - 6, 11):
                if rng.random() < 0.28:
                    c = np.array([1.0, 0.75, 0.42]) * rng.uniform(0.35, 0.9)
                    sky[wy:wy + 6, wx:wx + 5] = c
    save_rgb(np.clip(sky, 0, 1), path, 92)


# ---------------------------------------------------------------- story-location variants
def log_wall(path, npath, S=1024, knots=0.55):
    """horizontal pine logs (cabin): rounded rows, knots (darkening factor), warm amber"""
    y, x = np.mgrid[0:S, 0:S].astype(np.float32) / S
    rows = 5; v = (y * rows) % 1.0; idx = np.floor(y * rows).astype(int)
    round_ = np.sqrt(np.clip(1 - (2 * v - 1) ** 2, 0, 1))
    grain = fbm(S, S, 3, 5, 41)
    streak = np.asarray(Image.fromarray((grain * 255).astype(np.uint8)).resize((S, S // 16)).resize((S, S), Image.BICUBIC), np.float32) / 255
    tone = rng.uniform(0.85, 1.12, rows)[idx % rows]
    base = np.array([0.42, 0.25, 0.12])
    col = base[None, None] * (0.55 + 0.45 * round_[..., None]) * (0.8 + 0.3 * streak[..., None]) * tone[..., None]
    knot = fbm(S, S, 12, 2, 43) > 0.82
    col[knot] *= knots
    gap = v < 0.04
    col[gap] = np.array([0.12, 0.08, 0.05])
    save_rgb(col, path)
    save_rgb(normal_from_height(round_ * 0.6 + streak * 0.2 - gap * 0.6, 3.0), npath)

def plaster(path, npath, color=(0.86, 0.84, 0.79), S=1024):
    """white lime plaster (villa): soft trowel marks"""
    n = fbm(S, S, 3, 6, 51); m = fbm(S, S, 12, 3, 52)
    col = np.array(color)[None, None] * (0.93 + 0.08 * n[..., None] + 0.03 * m[..., None])
    save_rgb(col, path)
    save_rgb(normal_from_height(n * 0.25 + m * 0.1, 1.5), npath)

def plaid(path, npath, c1, c2, c3, S=512, seed=0):
    """wool plaid (cabin blankets)"""
    y, x = np.mgrid[0:S, 0:S].astype(np.float32) / S
    bx = (np.sin(x * 2 * np.pi * 4) > 0.3); by = (np.sin(y * 2 * np.pi * 4) > 0.3)
    thin = (np.abs(((x * 8) % 1.0) - 0.5) < 0.04) | (np.abs(((y * 8) % 1.0) - 0.5) < 0.04)
    col = np.broadcast_to(np.array(c1), (S, S, 3)).copy()
    col[bx ^ by] = np.array(c2) * 0.9 + np.array(c1) * 0.1
    col[bx & by] = np.array(c2)
    col[thin] = np.array(c3)
    n = fbm(S, S, 16, 3, seed)
    col *= (0.85 + 0.25 * n[..., None])
    save_rgb(col, path)
    save_rgb(normal_from_height(n * 0.5, 2.0), npath)

def snow_night(path, W=2048, H=1024):
    """snowy pine slope at night, falling snow (cabin window)"""
    y, x = np.mgrid[0:H, 0:W].astype(np.float32); u, v = x / W, y / H
    sky = np.dstack([0.03 + 0.05 * v, 0.04 + 0.06 * v, 0.08 + 0.09 * v])
    ground = v > 0.55 + 0.08 * np.sin(u * 7)
    sky[ground] = np.array([0.30, 0.33, 0.40]) * (0.8 + 0.3 * fbm(H, W, 10, 3, 61)[ground])[:, None]
    for k in range(60):                                   # pines
        cx = rng.uniform(0, W); base = H * rng.uniform(0.55, 0.7); h = rng.uniform(80, 220)
        tri = (y < base) & (y > base - h) & (np.abs(x - cx) < (y - (base - h)) * 0.35)
        sky[tri] = np.array([0.02, 0.035, 0.03])
    flakes = (rng.random((H, W)) > 0.996) * rng.uniform(0.5, 1.0, (H, W))
    sky += flakes[..., None] * 0.8
    save_rgb(np.clip(sky, 0, 1), path, 92)

def sea_day(path, W=2048, H=1024):
    """bright Mediterranean bay: sky, sea, distant headland (villa window)"""
    y, x = np.mgrid[0:H, 0:W].astype(np.float32); u, v = x / W, y / H
    sky = np.dstack([0.45 + 0.35 * v, 0.68 + 0.22 * v, 0.95 + 0.03 * v])
    sea = v > 0.52
    s_ = fbm(H, W, 20, 3, 71)
    sky[sea] = (np.array([0.05, 0.32, 0.50]) + 0.08 * s_[sea][:, None])
    land = (v > 0.46) & (v < 0.53) & (u > 0.55) & (v > 0.53 - 0.05 * np.sin((u - 0.55) * 9))
    sky[land] = np.array([0.42, 0.45, 0.30])
    save_rgb(np.clip(sky, 0, 1), path, 92)

def sea_night(path, W=2048, H=1024):
    """the sea at night under the moon (hotel window)"""
    night_sky(path, W, H)
    a = np.asarray(Image.open(path), np.float32) / 255.0
    yy = np.arange(H)[:, None] / H
    sea = np.broadcast_to(yy > 0.6, a.shape[:2])
    s_ = fbm(H, W, 30, 3, 81)
    glint = np.exp(-((np.arange(W)[None, :] / W - 0.72) / 0.05) ** 2) * (s_ > 0.55)
    a[sea] = (np.array([0.01, 0.02, 0.05]) + 0.02 * s_[sea][:, None] + 0.5 * glint[sea][:, None] * np.array([0.9, 0.92, 1.0]))
    save_rgb(np.clip(a, 0, 1), path, 92)

def fieldstone(path, npath, S=1024, seed=7):
    """rounded fieldstones in grey-brown mortar (hearths, chimney breasts); tileable"""
    r = np.random.default_rng(seed)
    pts = r.random((64, 2)) * S
    yy, xx = np.mgrid[0:S, 0:S].astype(np.float32)
    d1 = np.full((S, S), 1e9, np.float32); d2 = d1.copy(); idx = np.zeros((S, S), np.int32)
    for i, (px, py) in enumerate(pts):
        for ox in (-S, 0, S):
            for oy in (-S, 0, S):
                d = np.hypot(xx - (px + ox), (yy - (py + oy)) * 1.35)          # stones wider than tall
                closer = d < d1
                d2 = np.where(closer, d1, np.minimum(d2, d)); idx = np.where(closer, i, idx); d1 = np.where(closer, d, d1)
    edge = (d2 - d1) * 0.5                                               # distance to the stone's border (px)
    mortar = np.clip(1.0 - edge / 5.0, 0, 1)
    dome = np.sqrt(np.clip(edge / 26.0, 0, 1))
    n = fbm(S, S, 24, 4, seed + 1)
    tint = r.uniform(0.75, 1.15, (64, 3)) * np.array([1.0, 0.96, 0.9])
    stone = np.array([0.36, 0.33, 0.30])[None, None] * tint[idx] * (0.8 + 0.35 * n[..., None]) * (0.75 + 0.25 * dome[..., None])
    col = stone * (1 - mortar[..., None]) + np.array([0.20, 0.18, 0.16]) * (0.8 + 0.3 * n[..., None]) * mortar[..., None]
    save_rgb(np.clip(col, 0, 1), path)
    save_rgb(normal_from_height(dome * 0.9 * (1 - mortar) + n * 0.15, 4.0), npath)


def flame(path, W=256, H=256):
    """three flame tongues on black (emissive card, not tiled): orange edges, gold core, tapering to tips"""
    y, x = np.mgrid[0:H, 0:W].astype(np.float32); u = x / W; v = 1 - y / H
    out = np.zeros((H, W), np.float32)
    for c, hgt, wid in ((0.5, 0.96, 0.17), (0.28, 0.66, 0.12), (0.73, 0.76, 0.13)):
        t = np.clip(v / hgt, 0, 1)
        w = wid * (1 - t) ** 0.75 * (0.6 + 0.4 * np.sin(np.pi * np.minimum(t * 1.7, 1)))
        d = np.abs(u - c - 0.035 * np.sin(t * 5 + c * 9) * t) / np.maximum(w, 1e-3)
        out = np.maximum(out, np.clip(1 - d, 0, 1) ** 1.2 * (v < hgt) * np.clip(v * 12, 0, 1))
    core = out ** 2.2
    col = np.dstack([np.clip(out * 1.15, 0, 1), np.clip(out ** 1.6 * 0.7 + core * 0.25, 0, 1), np.clip(core * 0.4, 0, 1)])
    save_rgb(col, path, 92)
