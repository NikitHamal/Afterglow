"""Kari's face identity pack, built without Blender.

The shipped packs (mei/amara/daniel) come out of MakeHuman via tools/model/identity/identity.py, which needs Blender and
the base game rig. Kari is built the same way the engine consumes a pack: a per-vertex position/normal delta over the
base GLB's own vertices, written in the AGID format scene/identity.cpp reads. The face is a weighted blend of the two
existing her-model identities (Mei's East Asian skull and lids, Amara's fuller features) plus a small analytic sculpt
for the reference's features: full red lips, soft full cheeks, a narrower jaw, a shorter chin, a slightly wider nose
base and brows a touch lower and straighter.

    python tools/model/identity/kari_identity.py native/assets/characters \
        <vivian.glb> <mei_identity.bin> <amara_identity.bin> <out kari_identity.bin>
"""
import json
import struct
import sys

import numpy as np

MESH_HEADERS = [("vivian_body", 0), ("vivian_eyes", 0), ("vivian_brows", 0), ("vivian_lashes", 0),
                ("vivian_teeth", 0), ("vivian_tongue", 0)]
BLEND = {"mei": 0.55, "amara": 0.30}


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
    out = np.zeros((a["count"], n), dt)
    if "bufferView" in a:
        bv = js["bufferViews"][a["bufferView"]]
        st = bv.get("byteStride", 0) or dt.itemsize * n
        o = bv.get("byteOffset", 0) + a.get("byteOffset", 0)
        raw = np.frombuffer(bn, np.uint8, st * (a["count"] - 1) + dt.itemsize * n, o)
        idx = np.arange(a["count"])[:, None] * st + np.arange(dt.itemsize * n)[None]
        out = raw[idx].copy().view(dt).reshape(-1, n)
    return out


def pack_entries(path):
    b = open(path, "rb").read()
    ver, jl = struct.unpack_from("<II", b, 4)
    head = json.loads(b[12:12 + jl])
    blob = b[12 + jl:]
    out = {}
    for m in head["meshes"]:
        n, off = m["n"], m["offset"]
        idx = np.frombuffer(blob, np.uint32, n, off)
        dp = np.frombuffer(blob, np.float32, n * 3, off + n * 4).reshape(-1, 3)
        dn = np.frombuffer(blob, np.float32, n * 3, off + n * 4 + n * 12).reshape(-1, 3)
        out[m["name"]] = (idx.astype(np.int64), dp.astype(np.float64), dn.astype(np.float64))
    return head, out


def full_delta(prim_count, entries):
    d = np.zeros((prim_count, 3))
    idx, dp, _ = entries
    d[idx] = dp
    return d


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0, 1)
    return t * t * (3 - 2 * t)


def main():
    out_dir, viv_path, mei_path, ama_path, out_path = sys.argv[1:6]
    js, bn = read_glb(viv_path)
    names = [n.get("name", "") for n in js["nodes"]]
    # body primitive
    body_mesh = next(i for i, m in enumerate(js["meshes"]) if m.get("name") == "vivian_body")
    prim = js["meshes"][body_mesh]["primitives"][0]
    P = acc(js, bn, prim["attributes"]["POSITION"]).astype(np.float64)
    J = acc(js, bn, prim["attributes"]["JOINTS_0"])
    W = acc(js, bn, prim["attributes"]["WEIGHTS_0"])
    skin = js["skins"][0]
    jn = [names[j] for j in skin["joints"]]
    head_j = jn.index("頭")
    neck_j = jn.index("首")
    wh = np.array([sum(W[v, k] for k in range(4) if J[v, k] == head_j) for v in range(len(P))])
    wn = np.array([sum(W[v, k] for k in range(4) if J[v, k] == neck_j) for v in range(len(P))])
    mask = np.clip(wh + 0.8 * wn, 0, 1)
    mask = mask * mask * (3 - 2 * mask)

    _, mei = pack_entries(mei_path)
    _, ama = pack_entries(ama_path)
    D = np.zeros_like(P)
    for w, pack in ((BLEND["mei"], mei), (BLEND["amara"], ama)):
        D += w * full_delta(len(P), pack["vivian_body"])

    # ---- the sculpt for Kari's own features, in model space (metres, Y up, +Z face) ----
    # landmarks measured from the mesh itself
    mouth = np.array([0.0, 1.5095, 0.1458])
    eyes = pack_entries(mei_path)[1]["vivian_eyes"][0]
    # eyes mesh vertices are only indices; use their positions from the GLB
    eyes_mesh = next(i for i, m in enumerate(js["meshes"]) if m.get("name") == "vivian_eyes")
    EP = acc(js, bn, js["meshes"][eyes_mesh]["primitives"][0]["attributes"]["POSITION"]).astype(np.float64)
    eye_r = EP[EP[:, 0] > 0].mean(0)
    eye_l = EP[EP[:, 0] < 0].mean(0)
    add = np.zeros_like(P)
    x, y, z = P[:, 0], P[:, 1], P[:, 2]
    # full lips: push the mouth region forward, upper lip a little more than lower
    d_mouth = np.linalg.norm(P - mouth, axis=1)
    lips = np.exp(-(d_mouth / 0.020) ** 2)
    add[:, 2] += 0.0026 * lips
    add[:, 1] += 0.0006 * lips * np.clip((y - mouth[1]) / 0.008, -1, 1)
    # soft full cheeks
    for sx in (-1, 1):
        c = np.array([sx * 0.036, 1.523, 0.125])
        f = np.exp(-(np.linalg.norm(P - c, axis=1) / 0.022) ** 2)
        add[:, 0] += sx * 0.0016 * f
        add[:, 2] += 0.0010 * f
    # narrower jaw, shorter chin
    low = smoothstep(1.518, 1.498, y) * smoothstep(0.018, 0.040, np.abs(x)) * (z > 0.09)
    add[:, 0] -= np.sign(x) * 0.0018 * low
    chin = np.exp(-(np.linalg.norm(P - np.array([0.0, 1.4975, 0.136]), axis=1) / 0.016) ** 2)
    add[:, 1] += 0.0010 * chin
    add[:, 2] += 0.0012 * chin
    # brows a touch lower and straighter, nose base slightly wider (keep the eye sockets still)
    brow = np.exp(-(((y - 1.5465) / 0.006) ** 2)) * (np.abs(x) < 0.045)
    add[:, 1] -= 0.0006 * brow
    nose = np.exp(-(np.linalg.norm(P - np.array([0.0, 1.5355, 0.148]), axis=1) / 0.012) ** 2)
    add[:, 0] += np.sign(x) * 0.0009 * nose * np.clip(np.abs(x) / 0.01, 0, 1)
    # never move the eyeballs themselves
    for e in (eye_l, eye_r):
        add *= 1.0 - np.exp(-(np.linalg.norm(P - e, axis=1) / 0.013) ** 2)[:, None]
    D = (D + add) * mask[:, None]

    # ---- follow-through meshes: sample the field at each vertex, eyes pinned ----
    field_idx = np.where(np.linalg.norm(D, axis=1) > 1e-6)[0]
    field_pts = P[field_idx]
    field_vec = D[field_idx]

    def sample(Q):
        out = np.zeros_like(Q)
        if len(field_pts) == 0:
            return out
        for i in range(0, len(Q), 512):
            q = Q[i:i + 512]
            d2 = ((q[:, None, :] - field_pts[None, :, :]) ** 2).sum(-1)
            k = np.argmin(d2, axis=1)
            out[i:i + 512] = field_vec[k]
        return out

    body_idx = np.where(mask > 0.02)[0]
    entries = [("vivian_body", P, body_idx, D[body_idx], np.zeros((len(body_idx), 3)))]
    for name in ("vivian_brows", "vivian_lashes", "vivian_teeth", "vivian_tongue"):
        mi = next((i for i, m in enumerate(js["meshes"]) if m.get("name") == name), None)
        if mi is None:
            continue
        Q = acc(js, bn, js["meshes"][mi]["primitives"][0]["attributes"]["POSITION"]).astype(np.float64)
        idx = np.arange(len(Q))
        entries.append((name, Q, idx, sample(Q), np.zeros((len(Q), 3))))
    mi = next((i for i, m in enumerate(js["meshes"]) if m.get("name") == "vivian_eyes"), None)
    if mi is not None:
        Q = acc(js, bn, js["meshes"][mi]["primitives"][0]["attributes"]["POSITION"]).astype(np.float64)
        idx = np.arange(len(Q))
        entries.append(("vivian_eyes", Q, idx, np.zeros_like(Q), np.zeros_like(Q)))   # eyeballs pinned

    head = {"model": "kari", "base": "vivian", "meshes": []}
    blob = bytearray()
    for name, Q, idx, dp, dn in entries:
        head["meshes"].append({"name": name, "prim": 0, "vertexCount": int(len(Q)), "n": int(len(idx)),
                               "offset": len(blob)})
        blob += idx.astype(np.uint32).tobytes()
        blob += dp.astype(np.float32).tobytes()
        blob += dn.astype(np.float32).tobytes()
    j = json.dumps(head).encode()
    with open(out_path, "wb") as f:
        f.write(b"AGID" + struct.pack("<II", 1, len(j)) + j + bytes(blob))
    print("wrote", out_path, len(j), "json", len(blob), "blob;",
          "body deltas", int((np.linalg.norm(D[body_idx], axis=1) > 1e-6).sum()),
          "max %.1f mm" % (np.linalg.norm(D, axis=1).max() * 1000))


if __name__ == "__main__":
    main()
