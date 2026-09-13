"""Localize bad-normal vertices: where on the body are the changed normals?"""
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


def biggest(j):
    best = None
    for m in j['meshes']:
        for pi, p in enumerate(m['primitives']):
            if 'POSITION' not in p['attributes']:
                continue
            c = j['accessors'][p['attributes']['POSITION']]['count']
            if best is None or c > best[0]:
                best = (c, m.get('name'), pi, p)
    return best


def get(path):
    j, bin_ = load(path)
    _, name, pi, p = biggest(j)
    return {
        'pos': read_acc(j, bin_, p['attributes']['POSITION']),
        'nrm': read_acc(j, bin_, p['attributes']['NORMAL']),
        'joints': read_acc(j, bin_, p['attributes']['JOINTS_0']) if 'JOINTS_0' in p['attributes'] else None,
        'name': name,
    }


A = get(sys.argv[1])
B = get(sys.argv[2])
pos, na, nb = A['pos'], A['nrm'], B['nrm']
print('mesh', A['name'], 'n', len(pos))

# bucket verts by coarse Y (height) and report bad-normal share per band
bands = {}
worst = []
for i in range(min(len(na), len(nb))):
    d = max(abs(x - y) for x, y in zip(na[i], nb[i]))
    x, y, z = pos[i]
    key = (round(y, 1), round(abs(x), 1))
    b = bands.setdefault(key, [0, 0])
    b[0] += 1
    if d > 0.05:
        b[1] += 1
    if d > 0.05:
        worst.append((d, i, (round(x, 3), round(y, 3), round(z, 3))))

print('--- bad-normal share by (height y, |x|) ---')
for k in sorted(bands):
    tot, bad = bands[k]
    if bad:
        print(f'  y={k[0]:+.1f} |x|={k[1]:.1f}  bad={bad}/{tot}  ({100.0*bad/tot:.0f}%)')

worst.sort(reverse=True)
print('--- 12 worst normals ---')
for d, i, p in worst[:12]:
    print(f'  delta={d:.3f} v{i} pos={p}')

# overall bbox of bad verts
if worst:
    xs = [p[2][0] for p in worst]; ys = [p[2][1] for p in worst]; zs = [p[2][2] for p in worst]
    print(f'bad-vert bbox x[{min(xs):.2f},{max(xs):.2f}] y[{min(ys):.2f},{max(ys):.2f}] z[{min(zs):.2f},{max(zs):.2f}]')
