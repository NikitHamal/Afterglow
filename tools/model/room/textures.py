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
