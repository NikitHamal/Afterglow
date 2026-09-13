"""Health-check stored vertex normals against geometry-derived normals.
A sane smooth-shaded mesh should have dot(stored, geometric) > 0 nearly everywhere.
Reports % flipped (dot<0) and % badly off (dot<0.5), overall and for the hand region.

Usage: python check_normals_health.py <glb> [--region y0 y1]
"""
import json, struct, sys

CT = {5120: ('b', 1), 5121: ('B', 1), 5122: ('h', 2), 5123: ('H', 2), 5125: ('I', 4), 5126: ('f', 4)}
NC = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}


def load(path):
    with open(path, 'rb') as f:
        data = f.read()
    _, _, length = struct.unpack('<III', data[:12])
    off, chunks = 12, []
    while off < length:
        clen, ctype = struct.unpack('<II', data[off:off + 8])
        chunks.append((ctype, data[off + 8:off + 8 + clen]))
        off += 8 + clen
    return json.loads(chunks[0][1].decode('utf-8')), chunks[1][1]


def read_acc(j, bin_, idx):
    a = j['accessors'][idx]
    bv = j['bufferViews'][a['bufferView']]
    fmt, sz = CT[a['componentType']]
    n = NC[a['type']]
    stride = bv.get('byteStride') or (sz * n)
    base = bv.get('byteOffset', 0) + a.get('byteOffset', 0)
    return [struct.unpack_from('<' + fmt * n, bin_, base + i * stride) for i in range(a['count'])]


def vsub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def vcross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def vlen(a):
    return (a[0] * a[0] + a[1] * a[1] + a[2] * a[2]) ** 0.5


def vnorm(a):
    l = vlen(a) or 1.0
    return (a[0] / l, a[1] / l, a[2] / l)


def check(path, region=None):
    j, bin_ = load(path)
    # pick the primitive with the most verts
    best = None
    for m in j['meshes']:
        for pi, p in enumerate(m['primitives']):
            if 'POSITION' not in p['attributes']:
                continue
            c = j['accessors'][p['attributes']['POSITION']]['count']
            if best is None or c > best[0]:
                best = (c, m.get('name'), pi, p)
    cnt, name, pi, p = best
    pos = read_acc(j, bin_, p['attributes']['POSITION'])
    nrm = read_acc(j, bin_, p['attributes']['NORMAL'])
    idx = [t[0] for t in read_acc(j, bin_, p['indices'])]

    acc = [[0.0, 0.0, 0.0] for _ in range(len(pos))]
    for t in range(0, len(idx) - 2, 3):
        i0, i1, i2 = idx[t], idx[t + 1], idx[t + 2]
        a, b, c = pos[i0], pos[i1], pos[i2]
        n = vcross(vsub(b, a), vsub(c, a))
        for i in (i0, i1, i2):
            acc[i][0] += n[0]; acc[i][1] += n[1]; acc[i][2] += n[2]

    tot = fl = off = 0
    reg_tot = reg_fl = 0
    for i in range(len(pos)):
        g = vnorm(acc[i])
        if vlen(acc[i]) < 1e-12:
            continue
        s = vnorm(nrm[i])
        d = g[0] * s[0] + g[1] * s[1] + g[2] * s[2]
        tot += 1
        if d < 0:
            fl += 1
        elif d < 0.5:
            off += 1
        if region:
            x, y, z = pos[i]
            if region[0] <= y <= region[1]:
                reg_tot += 1
                if d < 0:
                    reg_fl += 1
    out = {'mesh': name, 'verts': len(pos), 'tris': len(idx) // 3,
           'checked': tot,
           'flipped': fl, 'flipped_pct': round(100.0 * fl / max(tot, 1), 2),
           'off_gt60deg': off, 'off_pct': round(100.0 * off / max(tot, 1), 2)}
    if region:
        out['region_y'] = region
        out['region_checked'] = reg_tot
        out['region_flipped'] = reg_fl
        out['region_flipped_pct'] = round(100.0 * reg_fl / max(reg_tot, 1), 2)
    return out


if __name__ == '__main__':
    region = None
    if '--region' in sys.argv:
        i = sys.argv.index('--region')
        region = (float(sys.argv[i + 1]), float(sys.argv[i + 2]))
    print(json.dumps(check(sys.argv[1], region), indent=1))
