"""Decode and compare POSITION/NORMAL/TEXCOORD/WEIGHTS for the largest primitive
of each GLB. Answers: did the re-export change geometry or shading?"""
import json, struct, sys

CT = {5120: ('b', 1), 5121: ('B', 1), 5122: ('h', 2), 5123: ('H', 2), 5125: ('I', 4), 5126: ('f', 4)}
NC = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}


def load(path):
    with open(path, 'rb') as f:
        data = f.read()
    _, _, length = struct.unpack('<III', data[:12])
    off = 12
    chunks = []
    while off < length:
        clen, ctype = struct.unpack('<II', data[off:off + 8])
        chunks.append((ctype, data[off + 8:off + 8 + clen]))
        off += 8 + clen
    j = json.loads(chunks[0][1].decode('utf-8'))
    bin_ = chunks[1][1]
    return j, bin_


def read_acc(j, bin_, idx):
    a = j['accessors'][idx]
    bv = j['bufferViews'][a['bufferView']]
    fmt, sz = CT[a['componentType']]
    n = NC[a['type']]
    stride = bv.get('byteStride') or (sz * n)
    base = bv.get('byteOffset', 0) + a.get('byteOffset', 0)
    out = []
    for i in range(a['count']):
        o = base + i * stride
        out.append(struct.unpack_from('<' + fmt * n, bin_, o))
    return out


def biggest_prim(j):
    best = None
    for mi, m in enumerate(j['meshes']):
        for pi, p in enumerate(m['primitives']):
            pos = p['attributes'].get('POSITION')
            if pos is None:
                continue
            c = j['accessors'][pos]['count']
            if best is None or c > best[0]:
                best = (c, m.get('name'), pi, p)
    return best


def summarize(path):
    j, bin_ = load(path)
    cnt, mname, pi, p = biggest_prim(j)
    res = {'mesh': mname, 'prim': pi, 'count': cnt, 'attrs': sorted(p['attributes'])}
    for key in ['POSITION', 'NORMAL', 'TEXCOORD_0', 'WEIGHTS_0']:
        if key in p['attributes']:
            res[key] = read_acc(j, bin_, p['attributes'][key])
    if 'indices' in p:
        res['indices'] = read_acc(j, bin_, p['indices'])
    return res


A = summarize(sys.argv[1])
B = summarize(sys.argv[2])
print('A mesh=%s prim=%d count=%d attrs=%s' % (A['mesh'], A['prim'], A['count'], A['attrs']))
print('B mesh=%s prim=%d count=%d attrs=%s' % (B['mesh'], B['prim'], B['count'], B['attrs']))

for key in ['POSITION', 'NORMAL', 'TEXCOORD_0', 'WEIGHTS_0']:
    if key not in A or key not in B:
        print(f'{key}: missing (A={key in A} B={key in B})')
        continue
    a, b = A[key], B[key]
    n = min(len(a), len(b))
    maxd = 0.0
    nbad = 0
    for i in range(n):
        d = max(abs(x - y) for x, y in zip(a[i], b[i]))
        if d > maxd:
            maxd = d
        if d > 0.05:
            nbad += 1
    print(f'{key}: n={n} maxDelta={maxd:.6g} vertsDelta>0.05={nbad} ({100.0*nbad/n:.2f}%)')

# index comparison
if 'indices' in A and 'indices' in B:
    ia = [t[0] for t in A['indices']]
    ib = [t[0] for t in B['indices']]
    print(f'indices: A={len(ia)} B={len(ib)} identical={ia == ib}')
    if ia != ib:
        diff = sum(1 for x, y in zip(ia, ib) if x != y)
        print(f'  index mismatches in common prefix: {diff}')
