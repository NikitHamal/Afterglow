"""Apply an MPFB identity npz to the shipped GLB, without the base game rig.

identity.py (Blender+MPFB) produces B0 (the base MakeHuman body's vertices), D (base -> model delta) and proxy
deltas. identity_apply.py normally needs the base's game .blend, which no longer exists anywhere. This does the same
job numerically: fit the MakeHuman head onto the GLB's head with a similarity transform (trying the two forward-axis
conventions), then carry the delta by nearest vertex, and write an AGID pack that scene/identity.cpp loads.

    python tools/model/identity/apply_npz.py <vivian.glb> <kari_id.npz> <out kari_identity.bin>
"""
import json
import struct
import sys

import numpy as np
from scipy.spatial import cKDTree

MESHES = ["vivian_body", "vivian_eyes", "vivian_brows", "vivian_lashes", "vivian_teeth", "vivian_tongue"]


def read_glb(path):
    b = open(path, "rb").read()
    off, js, bn = 12, None, None
    while off < len(b):
        cl, ct = struct.unpack_from("<II", b, off)
        off += 8
        if ct == 0x4E4F534A:
            js = json.loads(b[off:off + cl])
        elif ct == 0x004E4942:
            bn = b[off:off + cl]
        off += cl
    return js, bn


def acc(js, bn, i):
    a = js["accessors"][i]
    n = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}[a["type"]]
    dt = np.dtype({5126: np.float32, 5123: np.uint16, 5121: np.uint8, 5125: np.uint32}[a["componentType"]])
    bv = js["bufferViews"][a["bufferView"]]
    o = bv.get("byteOffset", 0) + a.get("byteOffset", 0)
    st = bv.get("byteStride", 0) or dt.itemsize * n
    raw = np.frombuffer(bn, np.uint8, st * (a["count"] - 1) + dt.itemsize * n, o)
    idx = np.arange(a["count"])[:, None] * st + np.arange(dt.itemsize * n)[None]
    return raw[idx].copy().view(dt).reshape(-1, n).astype(np.float64)


def umeyama(src, dst):
    ms, md = src.mean(0), dst.mean(0)
    S, D = src - ms, dst - md
    U, sig, Vt = np.linalg.svd(D.T @ S / len(src))
    d = np.sign(np.linalg.det(U @ Vt))
    R = U @ np.diag([1, 1, d]) @ Vt
    s = (sig * np.diag([1, 1, d])).sum() / (S ** 2).sum() * len(src)
    return s, R, md - s * R @ ms


def main():
    glb_path, npz_path, out_path = sys.argv[1:4]
    js, bn = read_glb(glb_path)
    Z = np.load(npz_path)
    B0, D = Z["B0"].astype(np.float64), Z["D"].astype(np.float64)
    mask = Z["mask"].astype(np.float64)

    names = [n.get("name", "") for n in js["nodes"]]
    skin = js["skins"][0]
    jn = [names[j] for j in skin["joints"]]
    hj, nj = jn.index("頭"), jn.index("首")
    mesh_i = next(i for i, m in enumerate(js["meshes"]) if m.get("name") == "vivian_body")
    prim = js["meshes"][mesh_i]["primitives"][0]
    P = acc(js, bn, prim["attributes"]["POSITION"])
    J = acc(js, bn, prim["attributes"]["JOINTS_0"])
    W = acc(js, bn, prim["attributes"]["WEIGHTS_0"])
    wh = np.array([sum(W[v, k] for k in range(4) if J[v, k] == hj) for v in range(len(P))])
    wn = np.array([sum(W[v, k] for k in range(4) if J[v, k] == nj) for v in range(len(P))])
    hm = np.clip(wh + 0.8 * wn, 0, 1)
    hm = hm * hm * (3 - 2 * hm)
    head_mask = mask > 0.5
    glb_head = hm > 0.5
    B_head = B0[head_mask]
    G_head = P[glb_head]

    def icp(B, G, iters=12):
        # no index correspondence between MakeHuman and the game mesh: start from centroid + RMS radius, then
        # iterate nearest-neighbour correspondences (ICP) with a similarity fit
        s0 = np.sqrt((G ** 2).sum(1).mean() / max(1e-9, (B ** 2).sum(1).mean()))
        R = np.eye(3)
        t = G.mean(0) - s0 * (B.mean(0) @ R.T)
        s = s0
        for _ in range(iters):
            Bf = s * (B @ R.T) + t
            dist, idx = cKDTree(Bf).query(G)
            s, R, t = umeyama(B[idx], G)
        Bf = s * (B @ R.T) + t
        d = cKDTree(Bf).query(G)[0]
        return s, R, t, float(np.sqrt((d ** 2).mean()))

    best = None
    for name, M in (("z-up", np.array([[1., 0, 0], [0, 0, 1], [0, -1, 0]])),
                    ("z-up-flip", np.array([[1., 0, 0], [0, 0, -1], [0, 1, 0]])),
                    ("y-up", np.eye(3)),
                    ("y-up-flip", np.array([[1., 0, 0], [0, 1, 0], [0, 0, -1]])),
                    ("z-up-mirror", np.array([[-1., 0, 0], [0, 0, 1], [0, 1, 0]])),
                    ("z-up-flip-mirror", np.array([[-1., 0, 0], [0, 0, -1], [0, -1, 0]]))):
        B = B_head @ M.T
        s, R, t, err = icp(B, G_head)
        if best is None or err < best[0]:
            best = (err, name, M, s, R, t)
    err, conv, M, s, R, t = best
    print("axis fit:", conv, "rms %.2f mm" % (err * 1000), "scale %.4f" % s)

    # positions and deltas go through the same map as the fit: x -> s * (x @ M.T) @ R.T + t
    B_all = s * ((B0 @ M.T) @ R.T) + t
    D_all = s * ((D @ M.T) @ R.T)
    tree = cKDTree(B_all)

    def carry(Q, V):
        dist, idx = tree.query(Q, k=4)
        w = 1.0 / np.maximum(dist, 1e-6) ** 2
        w /= w.sum(axis=1, keepdims=True)
        return (V[idx] * w[..., None]).sum(axis=1)

    idx_body = np.where(hm > 0.02)[0]
    Dg = carry(P[idx_body], D_all)
    # the head's own bind position must not drift: subtract the cranium's mean shift, as identity.py does
    top = hm[idx_body] > 0.9
    drift = Dg[top].mean(0) if top.any() else np.zeros(3)
    Dg = (Dg - drift[None]) * hm[idx_body, None]
    entries = [("vivian_body", idx_body, Dg, np.zeros_like(Dg))]

    for name in MESHES[1:]:
        mi = next((i for i, m in enumerate(js["meshes"]) if m.get("name") == name), None)
        if mi is None:
            continue
        Q = acc(js, bn, js["meshes"][mi]["primitives"][0]["attributes"]["POSITION"])
        key = name.split("_", 1)[1]
        pk = "P_" + key
        if pk in Z:
            V = s * ((Z[pk].astype(np.float64) @ M.T) @ R.T)
            Dp = carry(Q, V) if key != "eyes" else np.zeros_like(Q)
        elif key == "eyes":
            Dp = np.zeros_like(Q)                      # eyeballs stay pinned
        else:
            # the proxy was not installed in MakeHuman: follow the face field at the same spot
            Dp = carry(Q, D_all)
        entries.append((name, np.arange(len(Q)), Dp, np.zeros_like(Q)))

    head = {"model": "kari", "base": "vivian", "meshes": []}
    blob = bytearray()
    for name, idx, dp, dn in entries:
        head["meshes"].append({"name": name, "prim": 0, "vertexCount": int(P.shape[0] if name == "vivian_body" else len(idx)),
                               "n": int(len(idx)), "offset": len(blob)})
        blob += idx.astype(np.uint32).tobytes()
        blob += dp.astype(np.float32).tobytes()
        blob += dn.astype(np.float32).tobytes()
    j = json.dumps(head).encode()
    with open(out_path, "wb") as f:
        f.write(b"AGID" + struct.pack("<II", 1, len(j)) + j + bytes(blob))
    print("wrote", out_path, len(j), "json", len(blob), "blob; max delta %.1f mm" % (np.linalg.norm(Dg, axis=1).max() * 1000))


if __name__ == "__main__":
    main()
