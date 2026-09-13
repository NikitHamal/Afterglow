"""Compare NORMALs across EVERY primitive of two GLBs, and report per-mesh
bad-normal share with the world-space region of the damaged vertices."""
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


def prims(path):
    j, bin_ = load(path)
    out = {}
    for m in j['meshes']:
        for pi, p in enumerate(m['primitives']):
            if 'POSITION' not in p['attributes'] or 'NORMAL' not in p['attributes']:
                continue
            key = (m.get('name'), pi, p.get('material'))
            out[key] = {
                'pos': read_acc(j, bin_, p['attributes']['POSITION']),
                'nrm': read_acc(j, bin_, p['attributes']['NORMAL']),
            }
    return out


A = prims(sys.argv[1])
B = prims(sys.argv[2])
print('A prims=%d  B prims=%d' % (len(A), len(B)))
total_bad = 0
total_v = 0
for key in sorted(A, key=lambda k: (k[0] or '')):
    if key not in B:
        print('  MISSING in B:', key)
        continue
    a, b = A[key], B[key]
    na, nb, pa = a['nrm'], b['nrm'], a['pos']
    n = min(len(na), len(nb))
    if n == 0:
        continue
    bad = 0
    maxd = 0.0
    bx = []
    for i in range(n):
        d = max(abs(x - y) for x, y in zip(na[i], nb[i]))
        if d > maxd:
            maxd = d
        if d > 0.05:
            bad += 1
            bx.append(pa[i])
    total_bad += bad
    total_v += n
    if bad:
        xs = [p[0] for p in bx]; ys = [p[1] for p in bx]; zs = [p[2] for p in bx]
        print('  %-28s prim%d mat%-3s verts=%-6d BAD=%-6d (%4.1f%%) maxd=%.2f  bbox x[%.2f,%.2f] y[%.2f,%.2f] z[%.2f,%.2f]'
              % (key[0], key[1], key[2], n, bad, 100.0 * bad / n, maxd,
                 min(xs), max(xs), min(ys), max(ys), min(zs), max(zs)))
    else:
        print('  %-28s prim%d mat%-3s verts=%-6d OK (maxd=%.4f)' % (key[0], key[1], key[2], n, maxd))
print('TOTAL bad %d / %d (%.2f%%)' % (total_bad, total_v, 100.0 * total_bad / max(total_v, 1)))
