"""Dump + diff GLB structure (JSON chunk) between two files."""
import json, struct, sys

def load(path):
    with open(path, 'rb') as f:
        data = f.read()
    magic, ver, length = struct.unpack('<III', data[:12])
    off = 12
    chunks = []
    while off < length:
        clen, ctype = struct.unpack('<II', data[off:off + 8])
        chunks.append((ctype, data[off + 8:off + 8 + clen]))
        off += 8 + clen
    j = json.loads(chunks[0][1].decode('utf-8'))
    return j, chunks, data

def summarize(path):
    j, chunks, data = load(path)
    out = {
        'bytes': len(data),
        'chunkTypes': [hex(c[0]) for c in chunks],
        'chunkLens': [len(c[1]) for c in chunks],
        'asset': j.get('asset'),
        'extensionsUsed': j.get('extensionsUsed'),
        'counts': {k: len(j.get(k, [])) for k in
                   ['accessors', 'bufferViews', 'buffers', 'meshes', 'nodes',
                    'materials', 'images', 'textures', 'samplers', 'skins', 'animations']},
    }
    prims = []
    for mi, m in enumerate(j.get('meshes', [])):
        for pi, p in enumerate(m.get('primitives', [])):
            prims.append({
                'mesh': m.get('name'), 'i': pi,
                'attrs': sorted(p.get('attributes', {}).keys()),
                'mode': p.get('mode', 4),
                'material': p.get('material'),
                'indices': p.get('indices'),
                'targets': len(p.get('targets', [])),
            })
    out['prims'] = prims
    out['materials'] = [m.get('name') for m in j.get('materials', [])]
    out['images'] = [{'name': i.get('name'), 'mime': i.get('mimeType'),
                      'bv': i.get('bufferView'), 'uri': (i.get('uri') or '')[:40]}
                     for i in j.get('images', [])]
    # accessor type/count signature (to spot changed normals/uvs)
    acc = []
    for a in j.get('accessors', []):
        acc.append((a.get('type'), a.get('componentType'), a.get('count')))
    out['accSig'] = acc
    return out

a = summarize(sys.argv[1])
b = summarize(sys.argv[2])
print('=== A', sys.argv[1])
print(json.dumps({k: v for k, v in a.items() if k != 'accSig'}, indent=1, ensure_ascii=False))
print('=== B', sys.argv[2])
print(json.dumps({k: v for k, v in b.items() if k != 'accSig'}, indent=1, ensure_ascii=False))

print('=== DIFFS ===')
for k in ['bytes', 'chunkLens', 'counts', 'asset', 'extensionsUsed', 'materials']:
    if a[k] != b[k]:
        print(f'  {k}: A={a[k]}')
        print(f'  {k}: B={b[k]}')
if a['prims'] != b['prims']:
    print('  prims differ:')
    for pa, pb in zip(a['prims'], b['prims']):
        if pa != pb:
            print('    A', pa)
            print('    B', pb)
if a['images'] != b['images']:
    print('  images differ:')
    for ia, ib in zip(a['images'], b['images']):
        if ia != ib:
            print('    A', ia)
            print('    B', ib)
if a['accSig'] != b['accSig']:
    print('  accSig len A=%d B=%d' % (len(a['accSig']), len(b['accSig'])))
    for i, (x, y) in enumerate(zip(a['accSig'], b['accSig'])):
        if x != y:
            print(f'    acc[{i}] A={x} B={y}')
else:
    print('  accSig IDENTICAL')
