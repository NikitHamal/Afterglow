# Marcus' short hair as a scalp-hugging shell with a soft hairline, spliced into marcus.glb (nothing else touched).
#   python3 tools/model/male/hair_shell.py native/assets/characters/marcus.glb [out.glb]
# The shipped shell was 407 vertices: its border read as a sawtooth above the ears and its hairline fringe (meant for
# COLOR_0.a) was dropped by the Blender exporter (alpha 1 everywhere). This rebuilds it from the body itself:
#   - the scalp = body vertices of the head whose normal ray meets the old shell (same hair area), that coverage
#     smoothed over the mesh into a hairline field f (0 skin .. 1 full hair);
#   - the head triangles touched by hair, subdivided once (~8 mm edges), lifted 2.5 mm at the hairline to 8.5 mm on
#     top along the skin normal (follows every skull shape of the body);
#   - COLOR_0.a = the soft hairline (f ramp), so only single strands of the strand texture reach the skin edge;
#   - UVs polar around the crown (strands = +V, radiating from it, 9 tiles round, wrap seam split), as before;
#   - weights 100 % 頭 like the old shell; weight_up / weight_dn deltas copied from the body (the face/skull
#     move with weight, the hair moves with them).
import json, struct, sys
import numpy as np

HEAD = "頭"


def read_glb(path):
    b = open(path, "rb").read()
    off, js, bn = 12, None, None
    while off < len(b):
        cl, ct = struct.unpack_from("<II", b, off); off += 8
        if ct == 0x4E4F534A: js = json.loads(b[off:off + cl])
        elif ct == 0x004E4942: bn = bytearray(b[off:off + cl])
        off += cl
    return js, bn


def write_glb(path, js, bn):
    while len(bn) % 4: bn.append(0)
    js["buffers"][0]["byteLength"] = len(bn)
    j = json.dumps(js, separators=(",", ":"), ensure_ascii=False).encode()
    j += b" " * ((4 - len(j) % 4) % 4)
    total = 12 + 8 + len(j) + 8 + len(bn)
    with open(path, "wb") as f:
        f.write(struct.pack("<III", 0x46546C67, 2, total))
        f.write(struct.pack("<II", len(j), 0x4E4F534A)); f.write(j)
        f.write(struct.pack("<II", len(bn), 0x004E4942)); f.write(bytes(bn))


def acc(js, bn, i):
    a = js["accessors"][i]
    dt = np.dtype({5126: np.float32, 5123: np.uint16, 5121: np.uint8, 5125: np.uint32}[a["componentType"]])
    n = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}[a["type"]]
    out = np.zeros((a["count"], n), dt)
    if "bufferView" in a:
        bv = js["bufferViews"][a["bufferView"]]
        o, st = bv.get("byteOffset", 0) + a.get("byteOffset", 0), bv.get("byteStride", 0) or dt.itemsize * n
        rows = np.frombuffer(bytes(bn), np.uint8, st * (a["count"] - 1) + dt.itemsize * n, o)
        idx = np.arange(a["count"])[:, None] * st + np.arange(dt.itemsize * n)[None, :]
        out = rows[idx].copy().view(dt).reshape(-1, n)
    if "sparse" in a:
        sp = a["sparse"]
        ib, vb = js["bufferViews"][sp["indices"]["bufferView"]], js["bufferViews"][sp["values"]["bufferView"]]
        idt = np.dtype({5123: np.uint16, 5125: np.uint32, 5121: np.uint8}[sp["indices"]["componentType"]])
        ii = np.frombuffer(bytes(bn), idt, sp["count"], ib.get("byteOffset", 0) + sp["indices"].get("byteOffset", 0))
        vv = np.frombuffer(bytes(bn), dt, sp["count"] * n, vb.get("byteOffset", 0) + sp["values"].get("byteOffset", 0)).reshape(-1, n)
        out = out.copy(); out[ii.astype(np.int64)] = vv
    return out


def add_accessor(js, bn, data, ctype, atype, target=None, minmax=False, normalized=False):
    data = np.ascontiguousarray(data)
    while len(bn) % 4: bn.append(0)
    off = len(bn); bn.extend(data.tobytes())
    bv = {"buffer": 0, "byteOffset": off, "byteLength": data.nbytes}
    if target: bv["target"] = target
    js["bufferViews"].append(bv)
    a = {"bufferView": len(js["bufferViews"]) - 1, "componentType": ctype, "count": int(data.shape[0]), "type": atype}
    if normalized: a["normalized"] = True
    if minmax:
        a["min"] = [float(x) for x in data.min(0)]; a["max"] = [float(x) for x in data.max(0)]
    js["accessors"].append(a)
    return len(js["accessors"]) - 1


def sstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0, 1); return t * t * (3 - 2 * t)


def ray_hits(P, D, tris, tmax):
    """for each ray (P + t D, 0 < t < tmax): does it meet any triangle? (Moller-Trumbore, chunked)"""
    hit = np.zeros(len(P), bool)
    v0, e1, e2 = tris[:, 0], tris[:, 1] - tris[:, 0], tris[:, 2] - tris[:, 0]
    for s in range(0, len(P), 256):
        p, d = P[s:s + 256, None, :], D[s:s + 256, None, :]
        h = np.cross(d, e2[None]); a = np.einsum("ijk,jk->ij", h, e1)
        ok = np.abs(a) > 1e-12
        f = 1.0 / np.where(ok, a, 1.0)
        sv = p - v0[None]; u = f * np.einsum("ijk,ijk->ij", sv, h)
        q = np.cross(sv, e1[None]); v = f * np.einsum("ijk,ijk->ij", d, q)
        t = f * np.einsum("jk,ijk->ij", e2, q)
        m = ok & (u >= 0) & (v >= 0) & (u + v <= 1) & (t > -0.002) & (t < tmax)
        hit[s:s + 256] = m.any(1)
    return hit


def main():
    src = sys.argv[1]; dst = sys.argv[2] if len(sys.argv) > 2 else src
    js, bn = read_glb(src)
    meshes = {m["name"]: i for i, m in enumerate(js["meshes"])}
    body, hair = js["meshes"][meshes["marcus_body"]], js["meshes"][meshes["marcus_hair"]]
    bp = body["primitives"][0]
    P = acc(js, bn, bp["attributes"]["POSITION"]).astype(np.float64)
    Nm = acc(js, bn, bp["attributes"]["NORMAL"]).astype(np.float64)
    J = acc(js, bn, bp["attributes"]["JOINTS_0"]); W = acc(js, bn, bp["attributes"]["WEIGHTS_0"])
    I = acc(js, bn, bp["indices"]).reshape(-1, 3).astype(np.int64)
    joints = [js["nodes"][j]["name"] for j in js["skins"][0]["joints"]]
    hj = joints.index(HEAD)
    head_w = (W * (J == hj)).sum(1)
    hp = hair["primitives"][0]
    HP = acc(js, bn, hp["attributes"]["POSITION"]).astype(np.float64)
    HI = acc(js, bn, hp["indices"]).reshape(-1, 3).astype(np.int64)
    old = HP[HI]
    # ---- the hair area on the scalp: head vertices whose outward normal ray meets the old shell
    cand = np.nonzero(head_w > 0.5)[0]
    cov = np.zeros(len(P))
    cov[cand] = ray_hits(P[cand], Nm[cand], old, 0.03)
    # smooth it over the mesh (vertex adjacency from the triangles): a hairline field, not a staircase
    nb = [[] for _ in range(len(P))]
    for a_, b_, c_ in I:
        nb[a_] += [b_, c_]; nb[b_] += [a_, c_]; nb[c_] += [a_, b_]
    nbv = [np.unique(x) if x else np.zeros(0, np.int64) for x in nb]
    f = cov.copy()
    for _ in range(4):
        f = np.array([(f[i] + f[nbv[i]].sum()) / (1 + len(nbv[i])) if len(nbv[i]) else f[i] for i in range(len(P))])
    f = np.where(head_w > 0.3, f, 0.0)
    tri_sel = I[(f[I].max(1) > 0.06) & (head_w[I].min(1) > 0.3)]
    print(f"[hair] scalp: {int(cov.sum())} covered vertices, {len(tri_sel)} triangles in the hair area")
    # ---- subdivide once (shared edge midpoints): ~8 mm edges, a hairline the fringe can follow smoothly
    verts = list(np.unique(tri_sel))
    vid = {v: i for i, v in enumerate(verts)}
    pos = [P[v] for v in verts]; nrm = [Nm[v] for v in verts]; ff = [f[v] for v in verts]
    src_pairs = [(v, v) for v in verts]
    mid = {}
    def midpoint(a_, b_):
        k = (min(a_, b_), max(a_, b_))
        if k not in mid:
            mid[k] = len(pos)
            pos.append((P[a_] + P[b_]) * 0.5); n_ = Nm[a_] + Nm[b_]; nrm.append(n_ / np.linalg.norm(n_))
            ff.append((f[a_] + f[b_]) * 0.5); src_pairs.append(k)
        return mid[k]
    tris = []
    for a_, b_, c_ in tri_sel:
        ab, bc, ca = midpoint(a_, b_), midpoint(b_, c_), midpoint(c_, a_)
        A, B, C = vid[a_], vid[b_], vid[c_]
        tris += [(A, ab, ca), (ab, B, bc), (ca, bc, C), (ab, bc, ca)]
    pos, nrm, ff = np.array(pos), np.array(nrm), np.array(ff)
    tris = np.array(tris, np.int64)
    # ---- lift off the skin: thin at the hairline and the sides, fuller on top
    top_y, ear_y = pos[:, 1].max(), np.percentile(pos[:, 1], 15)
    topness = np.clip((pos[:, 1] - ear_y) / max(top_y - ear_y, 1e-6), 0, 1)
    lift = 0.0025 + 0.006 * sstep(0.45, 0.95, ff) * topness
    shell = pos + nrm * lift[:, None]
    fringe = sstep(0.22, 0.7, ff)
    # ---- UVs: polar round the crown (above and a little behind the head's top), strands along +V from it
    c = shell.mean(0); c[1] = ear_y
    crown = shell[np.argmax(shell[:, 1] - 0.6 * shell[:, 2])]
    ax = (crown - c) / np.linalg.norm(crown - c)
    rel = shell - c; rel /= np.linalg.norm(rel, axis=1, keepdims=True)
    theta = np.arccos(np.clip(rel @ ax, -1, 1))
    e1 = np.cross(ax, [0, 0, 1.0]); e1 /= np.linalg.norm(e1); e2 = np.cross(ax, e1)
    phi = np.arctan2(rel @ e2, rel @ e1)
    U = phi / (2 * np.pi) * 9.0; V = theta * 1.25
    # the wrap seam: triangles spanning it get their own copies of the low side's vertices
    extra = []
    tris = tris.copy()
    for t in range(len(tris)):
        ph = phi[tris[t]]
        if ph.max() - ph.min() > np.pi:
            for k in range(3):
                if ph[k] < 0:
                    extra.append(tris[t, k]); tris[t, k] = len(shell) + len(extra) - 1
    dup = np.array(extra, np.int64) if extra else np.zeros(0, np.int64)
    shell = np.concatenate([shell, shell[dup]]); nrm = np.concatenate([nrm, nrm[dup]])
    fringe = np.concatenate([fringe, fringe[dup]]); U = np.concatenate([U, U[dup] + 9.0]); V = np.concatenate([V, V[dup]])
    src_pairs = src_pairs + [src_pairs[i] for i in dup]
    n = len(shell)
    # ---- morphs (the hair's own target list) from the body's deltas
    names = body["extras"]["targetNames"]
    deltas = []
    for tn in hair["extras"]["targetNames"]:
        d = acc(js, bn, bp["targets"][names.index(tn)]["POSITION"]).astype(np.float64)
        deltas.append(np.array([(d[a_] + d[b_]) * 0.5 for a_, b_ in src_pairs]))
    # ---- write: new accessors, the hair primitive points at them (old data stays unreferenced)
    F32, U16, U8, U32 = 5126, 5123, 5121, 5125
    newp = {"attributes": {
        "POSITION": add_accessor(js, bn, shell.astype(np.float32), F32, "VEC3", 34962, minmax=True),
        "NORMAL": add_accessor(js, bn, nrm.astype(np.float32), F32, "VEC3", 34962),
        "TEXCOORD_0": add_accessor(js, bn, np.stack([U, V], 1).astype(np.float32), F32, "VEC2", 34962),
        "COLOR_0": add_accessor(js, bn, np.concatenate([np.full((n, 3), 255), (fringe * 255 + 0.5)[:, None]], 1).astype(np.uint8),
                                U8, "VEC4", 34962, normalized=True),
        "JOINTS_0": add_accessor(js, bn, np.tile(np.array([hj, 0, 0, 0], np.uint8), (n, 1)), U8, "VEC4", 34962),
        "WEIGHTS_0": add_accessor(js, bn, np.tile(np.array([1, 0, 0, 0], np.float32), (n, 1)), F32, "VEC4", 34962)},
        "indices": add_accessor(js, bn, tris.reshape(-1).astype(np.uint32 if n > 65535 else np.uint16),
                                U32 if n > 65535 else U16, "SCALAR", 34963),
        "material": hp["material"], "mode": 4,
        "targets": [{"POSITION": add_accessor(js, bn, d.astype(np.float32), F32, "VEC3", 34962, minmax=True)} for d in deltas]}
    hair["primitives"][0] = newp
    write_glb(dst, js, bn)
    print(f"[hair] shell: {n} vertices, {len(tris)} triangles (was {len(HP)} / {len(HI)}); "
          f"fringe {int((fringe < 0.99).sum())} soft vertices -> {dst}")


if __name__ == "__main__":
    main()
