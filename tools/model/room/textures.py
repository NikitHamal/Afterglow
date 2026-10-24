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

def wood_planks(path, npath, S=1024, base=(0.30, 0.17, 0.10)):
    """warm walnut floor planks (or any wood by `base`), 6 planks across, staggered ends"""
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
    col = np.array(base)[None, None] * (0.75 + 0.35 * stretch[..., None]) * (0.9 + 0.12 * rings[..., None]) * tone[..., None]
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


def tiles(path, npath, color=(0.78, 0.74, 0.66), grout=(0.52, 0.49, 0.44), n=2, S=1024, seed=21):
    """honed stone tiles (spa floors / walls): n x n per tile repeat, thin grout, soft veining, per-tile tone"""
    r = np.random.default_rng(seed)
    y, x = np.mgrid[0:S, 0:S].astype(np.float32) / S
    ix, iy = np.floor(x * n).astype(int), np.floor(y * n).astype(int)
    fx, fy = (x * n) % 1.0, (y * n) % 1.0
    g = 0.006 * n
    joint = (fx < g) | (fx > 1 - g) | (fy < g) | (fy > 1 - g)
    tone = r.uniform(0.94, 1.05, (n, n))[iy % n, ix % n]
    vein = fbm(S, S, 4, 5, seed + 1); fine = fbm(S, S, 40, 3, seed + 2)
    col = np.array(color)[None, None] * tone[..., None] * (0.92 + 0.1 * vein[..., None] + 0.04 * fine[..., None])
    col[joint] = np.array(grout)
    save_rgb(np.clip(col, 0, 1), path)
    save_rgb(normal_from_height(fine * 0.15 - joint * 0.5, 2.0), npath)


def paper_screen(path, W=512, H=1024, cols=4, rows=8):
    """a shoji screen lit from behind: warm paper glow through a dark wooden lattice (emissive, not tiled)"""
    y, x = np.mgrid[0:H, 0:W].astype(np.float32); u, v = x / W, y / H
    glow = 0.82 + 0.18 * np.exp(-((u - 0.5) ** 2 + (v - 0.45) ** 2) / 0.18)
    fib = fbm(H, W, 30, 3, 91)
    paper = np.dstack([1.0 * glow, 0.82 * glow, 0.58 * glow]) * (0.94 + 0.08 * fib[..., None])
    bar = (np.abs(((u * cols) % 1.0) - 0.5) > 0.47) | (np.abs(((v * rows) % 1.0) - 0.5) > 0.475) | (u < 0.03) | (u > 0.97) | (v < 0.02) | (v > 0.98)
    paper[bar] = np.array([0.05, 0.03, 0.02])
    save_rgb(np.clip(paper, 0, 1), path, 92)


def woven(path, npath, color=(0.72, 0.62, 0.42), S=512, strands=16, seed=41):
    """a woven straw / seagrass mat: over-under strands with fibre streaks (spa floors)"""
    y, x = np.mgrid[0:S, 0:S].astype(np.float32) / S
    cx, cy = np.floor(x * strands).astype(int), np.floor(y * strands).astype(int)
    over = (cx + cy) % 2 == 0
    fu, fv = (x * strands) % 1.0, (y * strands) % 1.0
    along = np.where(over, fv, fu)                         # position across the visible strand
    bump = np.sin(np.pi * along) ** 0.6
    fib = fbm(S, S, 64, 3, seed)
    streak = np.where(over, np.asarray(Image.fromarray((fib * 255).astype(np.uint8)).resize((S, S // 8)).resize((S, S)), np.float32),
                      np.asarray(Image.fromarray((fib * 255).astype(np.uint8)).resize((S // 8, S)).resize((S, S)), np.float32)) / 255
    col = np.array(color)[None, None] * (0.62 + 0.38 * bump[..., None]) * (0.85 + 0.25 * streak[..., None])
    save_rgb(np.clip(col, 0, 1), path)
    save_rgb(normal_from_height(bump * 0.8 + streak * 0.2, 3.0), npath)


FONTS = __import__("os").path.join(__import__("os").path.dirname(__import__("os").path.abspath(__file__)), "../../../native/res/fonts")


def _font(name, size):
    from PIL import ImageFont
    return ImageFont.truetype(f"{FONTS}/{name}", size)


def city_night(path, W=2048, H=1024, seed=5):
    """a city at night seen from a few floors up: blue hour sky, skyline of lit towers, nearer blocks (windows)"""
    r = np.random.default_rng(seed)
    y, x = np.mgrid[0:H, 0:W].astype(np.float32); v = y / H
    sky = np.dstack([0.05 + 0.20 * v ** 2, 0.06 + 0.10 * v ** 2, 0.16 + 0.06 * v])           # deep blue, warm haze low
    img = np.clip(sky, 0, 1)
    for layer, (base, hmin, hmax, wmin, wmax, lit, tone) in enumerate((
            (0.62, 0.10, 0.36, 18, 60, 0.18, 0.05), (0.78, 0.08, 0.30, 40, 120, 0.32, 0.035), (0.96, 0.10, 0.42, 90, 260, 0.45, 0.025))):
        xx = 0
        while xx < W:
            w = int(r.uniform(wmin, wmax)); h = int(H * r.uniform(hmin, hmax)); top = int(H * base) - h
            x0, x1 = xx, min(W, xx + w)
            img[max(0, top):int(H * base) + (H if layer == 2 else 0), x0:x1] = tone
            # windows: a grid, some lit (warm / cool)
            cw, ch = (3, 4) if layer == 0 else ((5, 7) if layer == 1 else (9, 13))
            for wy in range(max(0, top) + ch, min(H, int(H * base) + (H if layer == 2 else 0)) - ch, ch * 2):
                for wx in range(x0 + cw, x1 - cw, cw * 2):
                    if r.random() < lit:
                        c = np.array([1.0, 0.78, 0.45]) if r.random() < 0.75 else np.array([0.75, 0.85, 1.0])
                        img[wy:wy + ch, wx:wx + cw] = c * r.uniform(0.55, 1.0)
            if layer == 0 and r.random() < 0.15 and top > 4:
                img[top - 3:top, x0 + w // 2 - 1:x0 + w // 2 + 2] = np.array([1.0, 0.1, 0.05])      # aviation light
            xx += w + int(r.uniform(0, 12))
    glow = np.exp(-((v - 0.78) / 0.12) ** 2)[..., None] * np.array([0.10, 0.06, 0.03])
    save_rgb(np.clip(img + glow, 0, 1), path, 92)


def plaque(path, text, S=256):
    """an engraved brass door number"""
    from PIL import Image as I, ImageDraw
    n = fbm(S // 2, S, 6, 3, 61)
    base = np.dstack([0.72 + 0.1 * n, 0.55 + 0.08 * n, 0.26 + 0.05 * n])
    im = I.fromarray((np.clip(base, 0, 1) * 255).astype(np.uint8))
    d = ImageDraw.Draw(im)
    f = _font("CormorantGaramond-SemiBold.ttf", int(S * 0.36))
    tw, th = d.textbbox((0, 0), text, font=f)[2:]
    d.text(((S - tw) / 2, (S // 2 - th) / 2 - S * 0.03), text, font=f, fill=(48, 34, 16))
    im.save(path, quality=92)


def cardboard(path, label, S=512, seed=71):
    """a moving box side: kraft board, a strip of tape, a marker label"""
    from PIL import Image as I, ImageDraw
    y, x = np.mgrid[0:S, 0:S].astype(np.float32) / S
    n = fbm(S, S, 20, 4, seed)
    flutes = 0.5 + 0.5 * np.sin(x * S * 0.9)
    col = np.array([0.58, 0.42, 0.26])[None, None] * (0.88 + 0.12 * n[..., None] + 0.03 * flutes[..., None])
    tape = np.abs(y - 0.06) < 0.05
    col[tape] = np.array([0.66, 0.55, 0.38]) * (0.95 + 0.1 * n[tape][:, None])
    im = I.fromarray((np.clip(col, 0, 1) * 255).astype(np.uint8))
    d = ImageDraw.Draw(im)
    lines = label.split("|")
    f = _font("Inter-SemiBold.ttf", int(S * (0.13 if len(lines) == 1 else 0.1)))
    yy = S * 0.42 - len(lines) * S * 0.06
    for ln in lines:
        tw = d.textbbox((0, 0), ln, font=f)[2]
        d.text(((S - tw) / 2, yy), ln, font=f, fill=(28, 24, 22)); yy += S * 0.13
    im.save(path, quality=90)


def terrazzo(path, npath, base=(0.70, 0.67, 0.62), S=1024, seed=81):
    """stairwell terrazzo: chips of stone in a pale matrix"""
    r = np.random.default_rng(seed)
    col = np.broadcast_to(np.array(base), (S, S, 3)).copy() * (0.95 + 0.06 * fbm(S, S, 8, 3, seed)[..., None])
    h = np.zeros((S, S), np.float32)
    for _ in range(2600):
        cx, cy, rr = r.integers(0, S), r.integers(0, S), r.uniform(2, 9)
        c = r.choice([0.25, 0.45, 0.8, 0.95]) * np.array([1.0, r.uniform(0.85, 1.0), r.uniform(0.75, 0.95)])
        x0, x1, y0, y1 = int(max(0, cx - rr)), int(min(S, cx + rr + 1)), int(max(0, cy - rr)), int(min(S, cy + rr + 1))
        yy, xx = np.mgrid[y0:y1, x0:x1]
        m = (xx - cx) ** 2 + ((yy - cy) * 1.3) ** 2 < rr * rr
        col[y0:y1, x0:x1][m] = c; h[y0:y1, x0:x1][m] = 0.4
    save_rgb(np.clip(col, 0, 1), path)
    save_rgb(normal_from_height(h, 0.6), npath)


def facade(path, seed=1, bays=16, floors=8, lit=0.42, wall=(0.03, 0.03, 0.04), S=1024):
    """a city facade at night (emissive): window bays x floors over a square tile, lit windows warm / cool / TV-blue,
    some with curtains, the rest dark sky reflections"""
    r = np.random.default_rng(seed)
    img = np.broadcast_to(np.array(wall), (S, S, 3)).copy() * (0.8 + 0.4 * fbm(S, S, 8, 3, seed)[..., None])
    bw, fh = S / bays, S / floors
    for fy in range(floors):
        for bx in range(bays):
            x0, x1 = int(bx * bw + bw * 0.22), int((bx + 1) * bw - bw * 0.22)
            y0, y1 = int(fy * fh + fh * 0.25), int((fy + 1) * fh - fh * 0.18)
            if r.random() < lit:
                kind = r.random()
                c = np.array([1.0, 0.76, 0.44]) if kind < 0.7 else (np.array([0.78, 0.86, 1.0]) if kind < 0.92 else np.array([0.45, 0.6, 1.0]))
                c = c * r.uniform(0.45, 1.0)
                img[y0:y1, x0:x1] = c
                if r.random() < 0.4:                                   # a curtain half drawn
                    cx = x0 + int((x1 - x0) * r.uniform(0.3, 0.7))
                    img[y0:y1, x0:cx] = c * 0.45
            else:
                img[y0:y1, x0:x1] = np.array([0.04, 0.05, 0.08]) * r.uniform(0.8, 1.4)
    save_rgb(np.clip(img, 0, 1), path, 88)


def sketch(path, seed=3, S=512):
    """an architect's drawing on cartridge paper: a facade or plan in ink, some construction lines, a title block"""
    from PIL import Image as I, ImageDraw
    r = np.random.default_rng(seed)
    n = fbm(S, S, 10, 3, seed)
    paper = np.dstack([0.92 + 0.03 * n, 0.90 + 0.03 * n, 0.84 + 0.03 * n])
    im = I.fromarray((np.clip(paper, 0, 1) * 255).astype(np.uint8)); d = ImageDraw.Draw(im)
    ink = (34, 32, 40)
    for _ in range(18):                                     # faint construction lines
        y = int(r.uniform(0.08, 0.92) * S); d.line([(int(S * 0.05), y), (int(S * 0.95), y)], fill=(170, 168, 160), width=1)
    x0, y0, x1, y1 = int(S * 0.15), int(S * 0.18), int(S * 0.85), int(S * 0.78)
    if seed % 2:                                            # an elevation: storeys and windows, a roof line
        d.rectangle([x0, y0, x1, y1], outline=ink, width=3)
        fl = r.integers(3, 6); bays = r.integers(3, 7)
        for f in range(fl):
            for b in range(bays):
                wx = x0 + (b + 0.25) * (x1 - x0) / bays; wy = y0 + (f + 0.25) * (y1 - y0) / fl
                d.rectangle([wx, wy, wx + 0.5 * (x1 - x0) / bays, wy + 0.5 * (y1 - y0) / fl], outline=ink, width=2)
        d.line([(x0 - 10, y0), ((x0 + x1) / 2, y0 - S * 0.1), (x1 + 10, y0)], fill=ink, width=3)
    else:                                                   # a plan: rooms, doors swings
        d.rectangle([x0, y0, x1, y1], outline=ink, width=4)
        for _ in range(4):
            if r.random() < 0.5: xx = int(r.uniform(x0, x1)); d.line([(xx, y0), (xx, y1)], fill=ink, width=3)
            else: yy = int(r.uniform(y0, y1)); d.line([(x0, yy), (x1, yy)], fill=ink, width=3)
        for _ in range(3):
            cx, cy, rr = r.uniform(x0, x1), r.uniform(y0, y1), S * 0.06
            d.arc([cx - rr, cy - rr, cx + rr, cy + rr], 180, 270, fill=ink, width=2)
    d.rectangle([int(S * 0.62), int(S * 0.84), int(S * 0.92), int(S * 0.94)], outline=ink, width=2)
    im.save(path, quality=88)
