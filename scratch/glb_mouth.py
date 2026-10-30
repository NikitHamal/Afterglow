import json, struct, math, sys

p = sys.argv[1] if len(sys.argv) > 1 else 'native/assets/characters/vivian.glb'
d = open(p, 'rb').read()
o = 12; js = None; bin_ = None
while o < len(d):
    cl, ct = struct.unpack_from('<II', d, o); o += 8
    chunk = d[o:o+cl]; o += cl
    if ct == 0x4E4F534A: js = json.loads(chunk)
    elif ct == 0x004E4942: bin_ = chunk
acc = js['accessors']; bvs = js['bufferViews']

def comps_of(tp): return {'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4,'MAT4':16}[tp]
def fmt_of(ct): return {5126:'f',5125:'I',5123:'H',5121:'B',5122:'h',5120:'b'}[ct]

def read_acc(ai):
    a = acc[ai]; comps = comps_of(a['type']); fmt = fmt_of(a['componentType'])
    if 'bufferView' not in a:
        out = [(0.0,)*comps if comps > 1 else 0.0 for _ in range(a['count'])]
        if 'sparse' in a:
            sp = a['sparse']; ia = sp['indices']; va = sp['values']
            ibv = bvs[ia['bufferView']]
            ioff = ibv.get('byteOffset', 0) + ia.get('byteOffset', 0)
            ifmt = fmt_of(ia['componentType']); isz = struct.calcsize(ifmt)
            vbv = bvs[va['bufferView']]
            voff = vbv.get('byteOffset', 0) + va.get('byteOffset', 0)
            vfmt = fmt_of(a['componentType']); vsz = struct.calcsize(vfmt)
            for k in range(sp['count']):
                vi = struct.unpack_from('<' + ifmt, bin_, ioff + k * isz)[0]
                vals = struct.unpack_from('<' + vfmt * comps, bin_, voff + k * comps * vsz)
                out[vi] = vals if comps > 1 else vals[0]
        return out
    bv = bvs[a['bufferView']]; off = bv.get('byteOffset', 0) + a.get('byteOffset', 0)
    sz = struct.calcsize(fmt); stride = bv.get('byteStride', comps * sz)
    out = []
    for i in range(a['count']):
        vals = struct.unpack_from('<' + fmt * comps, bin_, off + i * stride)
        out.append(vals if comps > 1 else vals[0])
    return out

def mat_mul(A, B):
    return [sum(A[r*4+k]*B[k*4+c] for k in range(4)) for r in range(4) for c in range(4)]

def trs_mat(t, r, s):
    x, y, z, w = r
    xx, yy, zz = x*x, y*y, z*z
    m = [
        (1-2*(yy+zz))*s[0], (2*(x*y-z*w))*s[0], (2*(x*z+y*w))*s[0], t[0],
        (2*(x*y+z*w))*s[1], (1-2*(xx+zz))*s[1], (2*(y*z-x*w))*s[1], t[1],
        (2*(x*z-y*w))*s[2], (2*(y*z+x*w))*s[2], (1-2*(xx+yy))*s[2], t[2],
        0, 0, 0, 1]
    return m

parent = {}
for i, n in enumerate(js['nodes']):
    for c in n.get('children', []): parent[c] = i

def global_mat(i):
    n = js['nodes'][i]
    M = trs_mat(n.get('translation', [0,0,0]), n.get('rotation', [0,0,0,1]), n.get('scale', [1,1,1]))
    p = parent.get(i)
    return mat_mul(global_mat(p), M) if p is not None else M

def inv_rigid(M):
    # inverse of rotation*translation (scale 1)
    R = [[M[0], M[4], M[8]], [M[1], M[5], M[9]], [M[2], M[6], M[10]]]
    t = [M[3], M[7], M[11]]
    Rt = [[R[c][r] for c in range(3)] for r in range(3)]
    it = [-sum(Rt[r][k]*t[k] for k in range(3)) for r in range(3)]
    return [Rt[0][0],Rt[0][1],Rt[0][2],it[0], Rt[1][0],Rt[1][1],Rt[1][2],it[1], Rt[2][0],Rt[2][1],Rt[2][2],it[2], 0,0,0,1]

def xp(M, p):
    return tuple(sum(M[r*4+k]*p[k] for k in range(3)) + M[r*4+3] for r in range(3))

# node ids by name
by_name = {n.get('name'): i for i, n in enumerate(js['nodes'])}
head = by_name.get('頭')
print('head node', head)
H = global_mat(head)
Hinv = inv_rigid(H)

# choose the body mesh with the kiss morph
for mi, m in enumerate(js['meshes']):
    names = m.get('extras', {}).get('targetNames', [])
    if 'kiss' not in names: continue
    ti = names.index('kiss')
    for pi, pr in enumerate(m['primitives']):
        t = pr.get('targets', [{}])[ti] if pr.get('targets') else {}
        if 'POSITION' not in t: continue
        pos = read_acc(pr['attributes']['POSITION'])
        deltas = read_acc(t['POSITION'])
        maxd = max(math.sqrt(sum(c*c for c in dv)) for dv in deltas)
        strong = [i for i, dv in enumerate(deltas) if math.sqrt(sum(c*c for c in dv)) > maxd*0.4]
        if len(strong) < 50: continue
        print('mesh', mi, m.get('name'), 'prim', pi, 'strong', len(strong), 'maxd', round(maxd, 5))
        xs = [pos[i][0] for i in strong]; ys = [pos[i][1] for i in strong]; zs = [pos[i][2] for i in strong]
        xc = sum(xs)/len(xs)
        ctr = (xc, sum(ys)/len(ys), sum(zs)/len(zs))
        print('model bbox x %.4f..%.4f y %.4f..%.4f z %.4f..%.4f' % (min(xs),max(xs),min(ys),max(ys),min(zs),max(zs)))
        print('model centroid %.4f %.4f %.4f' % ctr)
        print('head local centroid %.4f %.4f %.4f' % xp(Hinv, ctr))
        mid = [i for i in strong if abs(pos[i][0]-xc) < 0.006]
        fz = max(pos[i][2] for i in mid)
        fy = [pos[i][1] for i in mid if pos[i][2] > fz-0.004]
        front = (xc, sum(fy)/len(fy), fz)
        print('front model %.4f %.4f %.4f  (n %d, y range %.4f..%.4f)' % (front[0], front[1], front[2], len(fy), min(fy), max(fy)))
        print('front head-local %.4f %.4f %.4f' % xp(Hinv, front))
        inner = (xc, sum(ys)/len(ys), min(zs))
        print('backmost model %.4f %.4f %.4f -> head local %.4f %.4f %.4f' % (inner + xp(Hinv, inner)))
        # aperture ring estimate: median y and z of the midline lip verts
        my = sorted(pos[i][1] for i in mid); mz = sorted(pos[i][2] for i in mid)
        med = (xc, my[len(my)//2], mz[len(mz)//2])
        print('median model %.4f %.4f %.4f -> head local %.4f %.4f %.4f' % (med + xp(Hinv, med)))
        # vertices near the median y (the actual mouth line), frontmost z there
        band = [i for i in mid if abs(pos[i][1]-med[1]) < 0.003]
        if band:
            bz = max(pos[i][2] for i in band)
            print('band y half-height lips front z %.4f (%d verts); centroid z %.4f' % (bz, len(band), sum(pos[i][2] for i in band)/len(band)))
