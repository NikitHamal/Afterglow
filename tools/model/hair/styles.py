# Hair styles on the kit (hairkit.py). Each style grows on the character's own head (game space) and returns the
# builder; rigid parts ride 頭, loose locks the rig's simulated chains (髪.<n>.a..d, Vivian's rig only).
import math
import numpy as np
from mathutils import Vector
from hairkit import Builder, HEAD, ss, nrm, tangent

SEGS = "abcd"
CH_U = np.array([0.22, 0.42, 0.62, 0.82, 1.0])

def chain_weights(ch, u):
    out = {}; head_w = float(ss(0.27, 0.12, u))
    if head_w > 0: out[HEAD] = head_w
    rest = 1.0 - head_w
    if rest <= 0: return out
    f = np.clip((u - CH_U[0]) / (CH_U[-1] - CH_U[0]), 0, 0.9999) * 4.0; j = int(f); t = f - j
    a = f"髪.{ch}.{SEGS[j]}"; b = f"髪.{ch}.{SEGS[min(j + 1, 3)]}"
    out[a] = out.get(a, 0) + rest * (1 - t)
    if t > 0: out[b] = out.get(b, 0) + rest * t
    return out

def chain_guide(h, ch, n=17):
    J = h.chain(ch)                                       # a, b, c, d, z
    root = J[0] + (J[0] - J[1]) * (CH_U[0] / (CH_U[1] - CH_U[0]))
    G = np.vstack([root, J]); uG = np.concatenate([[0.0], CH_U])
    u = np.linspace(0, 1, n)
    return np.stack([np.interp(u, uG, G[:, d]) for d in range(3)], 1)

def resample(P, n):
    seg = np.linalg.norm(np.diff(P, axis=0), axis=1); cl = np.concatenate([[0], np.cumsum(seg)])
    t = np.linspace(0, cl[-1], n); return np.stack([np.interp(t, cl, P[:, k]) for k in range(3)], 1), cl[-1]

def grow_on_skull(h, root, flow, off_fn, stop, seg=0.005, max_len=0.34, around_ears=True):
    """a strand that lies on the skull at the given lift, following flow(p, n) until stop(p, s)"""
    p = root.copy(); d = None; pts = []; s = 0.0
    while s < max_len:
        loc, n, _ = h.near(p)
        p = loc + n * off_fn(s, p)
        if around_ears:
            for c in h.EARC:
                dd = p - c; dd = dd - n * (dd @ n); L = np.linalg.norm(dd)
                if L < h.EAR_R: p = p + dd / max(L, 1e-6) * (h.EAR_R - L)
        if pts and stop(p, s): break
        pts.append(p.copy())
        f = flow(p, n)
        d = f if d is None else tangent(d * 0.75 + f * 0.25, n)   # strands turn gently (no shingles round the ears)
        p = p + d * seg; s += seg
    return np.array(pts)

def clump(h, S, R_, off, rng, frac=0.18, k_div=9):
    K = max(8, len(S) // k_div); uN = np.linspace(0, 1, S.shape[1])
    cent = R_[rng.choice(len(R_), K, replace=False)]
    for _ in range(10):
        lab = np.argmin(((R_[:, None] - cent[None]) ** 2).sum(-1), 1)
        cent = np.array([R_[lab == k].mean(0) if (lab == k).any() else cent[k] for k in range(K)])
    guides = np.array([S[lab == k].mean(0) if (lab == k).any() else np.zeros(S.shape[1:]) for k in range(K)])
    S = S + (guides[lab] - S) * (frac * ss(0.25, 0.9, uN))[None, :, None]
    for si in range(len(S)):
        for k in range(S.shape[1]):
            loc, n, _ = h.near(S[si, k]); hh = (S[si, k] - loc) @ n
            if hh < off * 0.8: S[si, k] = loc + n * off
    return S

def baby_hairs(b, h, flow, rng, n=300, phi_max=1.75, length=(0.008, 0.018), width=(0.0012, 0.0020), below=0.004, alpha=0.7,
               swirl=0.0):
    """fine short hairs on and just below the hairline: the soft edge real hairlines have"""
    k = h.k; made = 0; tries = 0
    while made < n and tries < n * 6:
        tries += 1
        ph = rng.uniform(-phi_max, phi_max)
        d = np.array([math.sin(ph), -math.cos(ph), 0.0])
        o = h.Ch + d * 0.2
        z = h.EC[2] + np.interp(abs(ph), h.HL_A, h.HL_Z) * k
        hit = h.bvh.ray_cast(Vector((o[0], o[1], z)), Vector(tuple(-d)), 0.3)
        if hit[0] is None: continue
        p0 = np.array(hit[0]); p0[2] = h.hairline_z(p0) - rng.uniform(0.0, below * k)
        loc, nn, _ = h.near(p0)
        if h.ear_dist(loc) < h.EAR_R + 0.006: continue
        L = rng.uniform(*length) * k; seg = L / 5
        pts = []; p = loc + nn * 0.0004; dd = None
        for i in range(6):
            loc, nn, _ = h.near(p); p = loc + nn * (0.0004 + 0.0008 * i / 5)
            pts.append(p.copy())
            f = flow(p, nn)
            if swirl: f = tangent(f + swirl * np.cross(nn, f) * math.sin(i * 1.3 + ph * 7), nn)
            dd = f if dd is None else tangent(dd * 0.5 + f * 0.5, nn)
            p = p + dd * seg
        P = np.array(pts); T = nrm(np.gradient(P, axis=0)); out = np.array([h.near(q)[1] for q in P])
        w = rng.uniform(*width) * k * (1 - 0.5 * np.linspace(0, 1, len(P)))
        b.card(P[::-1], nrm(np.cross(T, out))[::-1], w[::-1], 5, lambda u: {HEAD: 1.0}, alpha=alpha, v0=0.0)
        made += 1
    return made

def flyaways(b, h, roots, flow, rng, n=40, length=(0.06, 0.13), lift=(0.004, 0.012), alpha=0.85):
    sel = roots[rng.choice(len(roots), min(n, len(roots)), replace=False)]
    for r in sel:
        lf = rng.uniform(*lift) * h.k; L = rng.uniform(*length) * h.k
        P = grow_on_skull(h, r, flow, lambda s, p: 0.002 + lf * ss(0.0, 0.5 * L, s) * ss(L, 0.6 * L, s),
                          lambda p, s: False, seg=L / 10, max_len=L)
        if len(P) < 4: continue
        T = nrm(np.gradient(P, axis=0)); out = np.array([h.near(q)[1] for q in P])
        w = rng.uniform(0.0014, 0.0024) * h.k * (1 - 0.6 * np.linspace(0, 1, len(P)))
        b.card(P, nrm(np.cross(T, out)), w, 5, lambda u: {HEAD: 1.0}, alpha=alpha, twist_sd=30.0, outv=out)

# ============================================================================================== chignon (Vivian)
def chignon(h, rng):
    b = Builder(h); k = h.k
    AX = nrm(np.array([0.0, math.cos(math.radians(24)), -math.sin(math.radians(24))]))
    hit = h.bvh.ray_cast(Vector(tuple(h.Ch + AX * 0.02)), Vector(tuple(AX)), 0.3)
    back_pt = np.array(hit[0]) if hit[0] is not None else h.Ch + AX * 0.095
    E1 = nrm(np.array([1.0, 0, 0]) - AX * AX[0]); E2 = nrm(np.cross(AX, E1))
    if E2[2] < 0: E2 = -E2
    BUN_C = back_pt + AX * 0.019 * k; BUN_A = np.array([0.039, 0.030, 0.023]) * k
    def bun_pt(lam, phi, kk=1.0):
        a = BUN_A * kk
        return BUN_C + E1 * (a[0] * np.cos(lam) * np.cos(phi)) + E2 * (a[1] * np.cos(lam) * np.sin(phi)) + AX * (a[2] * np.sin(lam))
    def bun_n(lam, phi, kk=1.0):
        a = BUN_A * kk
        return nrm(E1 * (np.cos(lam) * np.cos(phi) / a[0]) + E2 * (np.cos(lam) * np.sin(phi) / a[1]) + AX * (np.sin(lam) / a[2]))
    def in_bun(p, pad=0.0):
        d = p - BUN_C; q = np.array([d @ E1, d @ E2, d @ AX]) / (BUN_A + pad); return float(q @ q) < 1.0
    ROOTS = h.roots(0.0046 * k, 0.0026 * k, 0.025 * k, keep=lambda P: np.array([not in_bun(p, 0.004) for p in P]))
    print(f"[style] chignon: {len(ROOTS)} roots", flush=True)
    # combed back: one back-and-down direction laid on the skull (no pole, so no starburst on the crown),
    # turning into the bun only near it
    DIR = nrm(np.array([0.0, 1.0, -0.35]))
    def flow(p, n):
        t = DIR - n * (n @ DIR)
        if np.linalg.norm(t) < 0.2: t = nrm(BUN_C - p) - n * (n @ nrm(BUN_C - p))
        dist = np.linalg.norm(p - BUN_C)
        wb = float(ss(0.075 * k, 0.035 * k, dist))
        t = nrm(t) * (1 - wb) + nrm(BUN_C - p) * wb
        t = tangent(t, n)
        for c in h.EARC:
            d = p - c; d = d - n * (d @ n); L = np.linalg.norm(d)
            if L < 0.075 * k:
                dh = d / max(L, 1e-6); bb = float(ss(0.075 * k, 0.045 * k, L))
                into = min(0.0, t @ dh); t = tangent(t - dh * into * bb + dh * 0.25 * bb, n)
        return t
    def on_face(p): return p[1] < h.EC[1] + 0.03 and p[2] < h.hairline_z(p, False) - 0.003
    NSEG = 9
    layers = [(0.0020, 1.0, 0.0122, (0, 1, 2)), (0.0042, 0.72, 0.0108, (0, 1, 2)),
              (0.0064, 0.42, 0.0086, (0, 1, 2, 3)), (0.0088, 0.14, 0.0064, (3, 4))]
    for li, (off, frac, width, cols) in enumerate(layers):
        idx = np.where(rng.random(len(ROOTS)) < frac)[0]; S = []; keep = []
        for i in idx:
            r = ROOTS[i] + (rng.normal(0, 0.0012, 3) if li else 0)
            crown = float(ss(h.EC[2] + 0.07 * k, h.TOP_Z - 0.01, r[2]))
            P = grow_on_skull(h, r, flow, lambda s, p, off=off, crown=crown: off * k + 0.006 * k * crown * ss(0.0, 0.05, s) * ss(0.22, 0.10, s),
                              lambda p, s: in_bun(p, 0.002) or on_face(p), seg=0.004)
            if len(P) < 4: continue
            P, ln = resample(P, NSEG + 1)
            if ln < 0.02: continue
            S.append(P); keep.append(i)
        S = clump(h, np.array(S), ROOTS[keep], off * k, rng)
        b.strand_cards(S, width * k, cols, taper=0.3, v0=0.0 if li == 0 else 0.16)
        print(f"[style] layer {li}: {len(S)} strands", flush=True)
    for shell, (kk, n_) in enumerate([(1.00, 220), (1.10, 180), (1.20, 110)]):
        for _ in range(n_):
            phi0 = rng.uniform(0, 2 * np.pi); turns = rng.uniform(0.9, 1.4) * (1 if shell != 1 else -1)
            lam0, lam1 = rng.uniform(-0.55, -0.15), rng.uniform(0.95, 1.40)
            t = np.linspace(0, 1, 13); lam = lam0 + (lam1 - lam0) * t; ph = phi0 + 2 * np.pi * turns * t
            P = np.array([bun_pt(a, c, kk) for a, c in zip(lam, ph)]); Nn = np.array([bun_n(a, c, kk) for a, c in zip(lam, ph)])
            T = nrm(np.gradient(P, axis=0)); Bv = nrm(np.cross(T, Nn))
            w = (0.0100 + 0.002 * rng.random()) * k * (1 - 0.25 * np.linspace(0, 1, len(P)))
            b.card(P, Bv, w, int(rng.choice((0, 1, 2, 3, 4))), lambda u: {HEAD: 1.0}, twist_sd=5.0, outv=Nn)
    for _ in range(14):                                   # a few strands escape the bun
        lam, ph = rng.uniform(-0.2, 1.0), rng.uniform(0, 2 * np.pi)
        p0 = bun_pt(lam, ph, 1.18); n0 = bun_n(lam, ph, 1.18)
        tdir = nrm(np.cross(n0, np.array([0, 0, 1.0])) * rng.choice([-1, 1]) + np.array([0, 0, -0.6]))
        L = rng.uniform(0.025, 0.05) * k
        P = np.array([p0 + tdir * L * t + n0 * 0.006 * k * math.sin(math.pi * t) for t in np.linspace(0, 1, 7)])
        T = nrm(np.gradient(P, axis=0))
        b.card(P, nrm(np.cross(T, n0[None].repeat(len(P), 0))), rng.uniform(0.0016, 0.0024) * k * np.linspace(1, 0.4, len(P)),
               5, lambda u: {HEAD: 1.0}, alpha=0.85, twist_sd=20.0, outv=n0[None].repeat(len(P), 0))
    nb = baby_hairs(b, h, flow, rng, n=200)
    flyaways(b, h, ROOTS[ROOTS[:, 2] > h.EC[2] + 0.05 * k], flow, rng, n=36)
    if h.has_chains():
        for ch in (0, 1):                                 # face-framing tendrils: real locks on the simulated chains
            b.lock(chain_guide(h, ch), 12, 0.0065 * k, 0.0032 * k, (3, 4), lambda u, ch=ch: chain_weights(ch, u), spread_tip=2.0, v0=0.2)
            b.lock(chain_guide(h, ch), 5, 0.010 * k, 0.0016 * k, (5,), lambda u, ch=ch: chain_weights(ch, u), alpha=0.85, spread_tip=2.4, v0=0.2)
        for ch in (2, 3):                                 # nape wisps
            b.lock(chain_guide(h, ch, 13), 8, 0.004 * k, 0.0024 * k, (5,), lambda u, ch=ch: chain_weights(ch, u), alpha=0.9, v0=0.2)
    def cap_uv(pv):
        d = pv - BUN_C; ang = math.atan2(d @ E2, d @ E1)
        return (3.0 * (ang / (2 * math.pi) + 0.5), np.linalg.norm(d) / 0.05)
    nc = b.cap(0.0015 * k, 0.004 * k, cap_uv)
    print(f"[style] chignon: {len(b.F)} faces ({nb} baby hairs, cap {nc})", flush=True)
    return b

# ============================================================================================== A-line bob (Mei)
def bob(h, rng, part_x=-0.022):
    b = Builder(h); k = h.k
    PART = part_x * k
    ROOTS = h.roots(0.0046 * k, 0.0026 * k, 0.025 * k)
    print(f"[style] bob: {len(ROOTS)} roots", flush=True)
    def end_z(phi):                                       # A-line: chin length at the face, short at the nape
        a = np.abs(phi)
        return h.CHIN_Z + np.interp(a, [0.0, 1.0, 1.6, 2.2, 2.7, math.pi], [0.01, 0.0, 0.012, 0.045, 0.075, 0.085]) * k
    def flow(p, n):
        side = 1.0 if p[0] >= PART else -1.0
        away = np.array([side, 0.0, 0.0])
        down = np.array([0.0, 0.0, -1.0])
        top = float(ss(h.Ch[2] + 0.03 * k, h.TOP_Z - 0.005, p[2]))
        f = away * (0.25 + 0.75 * top) + down * (1.0 - 0.6 * top)
        # front: the hair slides along above the hairline to the temple, never down the forehead
        if p[1] < h.EC[1] + 0.05 * k:
            near_line = float(ss(h.hairline_z(p, False) + 0.03 * k, h.hairline_z(p, False) + 0.008 * k, p[2]))
            f = f * (1 - near_line) + (away * 1.0 + np.array([0.0, 0.35, 0.0])) * near_line
            if abs(p[0]) < h.face_half(p[2]):
                f = f + np.array([0.0, 0.0, 0.25]) * near_line   # lift off the brow rather than over it
        return tangent(f, n)
    NSEG = 8
    def strand(r, off):
        ez = float(end_z(h.phi(r)))
        P = grow_on_skull(h, r, flow, lambda s, p: off, lambda p, s: p[2] < h.Ch[2] - 0.035 * k, seg=0.005, max_len=0.25)
        if len(P) < 3: return None
        # fall: from the skull's widest band straight down, clear of the face and neck, ends curl under
        p = P[-1].copy(); out = [*P]; d = np.array([0.0, 0.0, -1.0])
        rad = p[:2] - h.Ch[:2]; rad = rad / max(np.linalg.norm(rad), 1e-6)
        while p[2] > ez and len(out) < 80:
            loc, n, dist = h.near(p)
            clear = off + 0.004 * k
            if dist < clear: p = loc + n * clear
            # in front of the ears the hair hangs beside the cheeks, never across the face
            if p[1] < h.EC[1] + 0.06 * k:
                fh = h.face_half(p[2]) + 0.006 * k + off
                if abs(p[0]) < fh: p[0] = math.copysign(fh, p[0] if abs(p[0]) > 1e-4 else (1.0 if r[0] >= PART else -1.0))
            p = p + d * 0.005
            out.append(p.copy())
        out = np.array(out)
        if len(out) > 3:   # the last 1.5 cm turns under, toward the neck
            m = out[:, 2] < ez + 0.015 * k
            t = ss(ez + 0.015 * k, ez, out[:, 2])
            out[m, 0] -= (rad[0] * 0.006 * k * t[m]); out[m, 1] -= (rad[1] * 0.006 * k * t[m])
        P, ln = resample(out, NSEG + 1)
        return P if ln > 0.03 else None
    layers = [(0.0022, 1.0, 0.0118, (0, 1, 2)), (0.0046, 0.6, 0.0105, (0, 1, 2, 3)), (0.0072, 0.3, 0.0092, (0, 1, 2, 3))]
    for li, (off, frac, width, cols) in enumerate(layers):
        idx = np.where(rng.random(len(ROOTS)) < frac)[0]; S = []; keep = []
        for i in idx:
            P = strand(ROOTS[i] + (rng.normal(0, 0.001, 3) if li else 0), off * k)
            if P is not None: S.append(P); keep.append(i)
        S = clump(h, np.array(S), ROOTS[keep], off * k, rng, frac=0.22)
        b.strand_cards(S, width * k, cols, taper=0.18, v0=0.0 if li == 0 else 0.16)
        print(f"[style] bob layer {li}: {len(S)} strands", flush=True)
    nb = baby_hairs(b, h, flow, rng, n=220, phi_max=1.6)
    flyaways(b, h, ROOTS[ROOTS[:, 2] > h.EC[2] + 0.06 * k], flow, rng, n=22, length=(0.05, 0.1))
    def cap_uv(pv):
        ang = math.atan2(pv[1] - h.Ch[1], pv[0] - PART); return (3.0 * (ang / (2 * math.pi) + 0.5), (h.TOP_Z - pv[2]) / 0.05)
    nc = b.cap(0.0015 * k, -0.002 * k, cap_uv, fade_from=0.003 * k)
    print(f"[style] bob: {len(b.F)} faces ({nb} baby hairs, cap {nc})", flush=True)
    return b

# ============================================================================================== high puff (Amara)
def puff(h, rng):
    b = Builder(h); k = h.k
    PC = h.Ch + np.array([0.0, 0.03, 0.105]) * k          # the puff sits on the crown, a little back
    PA = np.array([0.072, 0.068, 0.058]) * k
    def in_puff(p, pad=0.0):
        q = (p - PC) / (PA + pad); return float(q @ q) < 1.0
    ROOTS = h.roots(0.0048 * k, 0.0028 * k, 0.02 * k, keep=lambda P: np.array([not in_puff(p, -0.012 * k) for p in P]))
    print(f"[style] puff: {len(ROOTS)} roots", flush=True)
    POLE = PC - np.array([0, 0, 0.02 * k])
    def flow(p, n):
        return tangent(POLE - p, n)
    NSEG = 8
    for li, (off, frac, width, cols) in enumerate([(0.0016, 1.0, 0.0090, (0, 1, 2)), (0.0034, 0.6, 0.0080, (0, 1, 2))]):
        idx = np.where(rng.random(len(ROOTS)) < frac)[0]; S = []; keep = []
        for i in idx:
            P = grow_on_skull(h, ROOTS[i], flow, lambda s, p, off=off: off * k, lambda p, s: in_puff(p, -0.004 * k))
            if len(P) < 3: continue
            P, ln = resample(P, NSEG + 1)
            if ln < 0.015: continue
            S.append(P); keep.append(i)
        S = clump(h, np.array(S), ROOTS[keep], off * k, rng, frac=0.12)
        b.strand_cards(S, width * k, cols, taper=0.25, v0=0.0 if li == 0 else 0.16)
        print(f"[style] puff sleek layer {li}: {len(S)} strands", flush=True)
    # the puff: coily cards on nested shells of the ellipsoid, plus some reaching out for an uneven silhouette
    count = 0
    for kk, n_ in [(0.80, 700), (0.92, 700), (1.02, 560), (1.10, 220)]:
        for _ in range(n_):
            u = rng.normal(0, 1, 3); u /= np.linalg.norm(u)
            if u[2] < -0.55: continue                     # not under the puff (it sits on the head)
            c = PC + u * PA * kk
            loc, nn, dist = h.near(c)
            if (c - loc) @ nn < 0.004 * k: continue
            t = nrm(np.cross(u, rng.normal(0, 1, 3)))
            L = rng.uniform(0.022, 0.04) * k
            P = np.array([c + t * L * (s - 0.5) + u * 0.004 * k * math.sin(math.pi * s) for s in np.linspace(0, 1, 6)])
            T = nrm(np.gradient(P, axis=0)); Bv = nrm(np.cross(T, u[None].repeat(len(P), 0)))
            w = rng.uniform(0.012, 0.02) * k * np.ones(len(P))
            b.card(P, Bv, w, 6, lambda uu: {HEAD: 1.0}, twist_sd=25.0, outv=u[None].repeat(len(P), 0))
            count += 1
    nb = baby_hairs(b, h, flow, rng, n=260, phi_max=1.9, length=(0.008, 0.02), swirl=0.9)
    def cap_uv(pv):
        d = pv - POLE; ang = math.atan2(d[1], d[0]); return (3.0 * (ang / (2 * math.pi) + 0.5), np.linalg.norm(d) / 0.05)
    nc = b.cap(0.0012 * k, -0.002 * k, cap_uv, fade_from=0.002 * k)
    print(f"[style] puff: {count} puff cards, {len(b.F)} faces ({nb} edges, cap {nc})", flush=True)
    return b

# ============================================================================================== textured crop (Daniel)
def crop(h, rng):
    b = Builder(h); k = h.k
    ROOTS = h.roots(0.0042 * k, 0.003 * k, 0.02 * k)
    top_c = h.Ch + np.array([0.0, -0.02, 0.10]) * k
    def is_top(p): return p[2] > h.EC[2] + 0.055 * k and abs(h.phi(p)) < 1.9
    def flow(p, n):
        if is_top(p): return tangent(np.array([0.0, 0.75, 0.35]) + (p - top_c) * np.array([0.5, 0, 0]) / (0.05 * k), n)
        return tangent(np.array([0.0, 0.25, -1.0]), n)
    NSEG = 6
    S = []; keep = []
    # top cards root well inside the hairline: the cap's fade makes the edge, no card start shows at the front
    tops = np.where([is_top(r) and r[2] > h.hairline_z(r, False) + 0.009 * k for r in ROOTS])[0]
    for i in tops:
        L = rng.uniform(0.022, 0.034) * k
        P = grow_on_skull(h, ROOTS[i], flow, lambda s, p: 0.003 * k + 0.005 * k * ss(0.0, 0.015, s),
                          lambda p, s: False, seg=L / 8, max_len=L)
        if len(P) < 4: continue
        P, ln = resample(P, NSEG + 1); S.append(P); keep.append(i)
    S = clump(h, np.array(S), ROOTS[keep], 0.003 * k, rng, frac=0.3, k_div=6)
    keep_top = rng.random(len(S)) < 0.7
    b.strand_cards(S[keep_top], 0.0068 * k, (7,), taper=0.5, twist_sd=4.0, v0=0.12)
    print(f"[style] crop: {len(S)} top strands", flush=True)
    nb = 0   # a man's short hairline is crisp: the cap's own fade does it
    def cap_uv(pv):
        d = pv - top_c; ang = math.atan2(d[1], d[0]); return (6.0 * (ang / (2 * math.pi) + 0.5), np.linalg.norm(d) / 0.03)
    nc = b.cap(0.0022 * k, -0.004 * k, cap_uv, fade_from=0.001 * k, fade_len=0.007)
    print(f"[style] crop: {len(b.F)} faces ({nb} hairline, cap {nc})", flush=True)
    return b

STYLES = {"chignon": chignon, "bob": bob, "puff": puff, "crop": crop}
