# Skin weights check for the shipped GLBs (characters + outfits): what makes skinning tear or drag in some pose.
#   python3 tools/model/native/weights_check.py native/assets/characters/*.glb native/assets/characters/outfits/*.glb
# Per skinned primitive: weight sums off 1 (unnormalised: the vertex shrinks / swells with every bone), more than
# four influences (a second JOINTS_1 set the engine ignores), and stray influences — a bone weighted > 5 % on a vertex
# farther from that bone's segment (head -> child) than `--stray` metres (default 0.25; a thigh vertex pulled by the
# hand). Exit status 1 when anything is found.
import json, struct, sys
import numpy as np


def load(path):
    b = open(path, "rb").read()
    off, js, bn = 12, None, None
    while off < len(b):
        cl, ct = struct.unpack_from("<II", b, off); off += 8
        if ct == 0x4E4F534A: js = json.loads(b[off:off + cl])
        elif ct == 0x004E4942: bn = b[off:off + cl]
        off += cl
    return js, bn


def acc(js, bn, i):
    a = js["accessors"][i]; bv = js["bufferViews"][a["bufferView"]]
    nc = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}[a["type"]]
    dt = np.dtype({5126: np.float32, 5123: np.uint16, 5125: np.uint32, 5121: np.uint8}[a["componentType"]])
    st = bv.get("byteStride", 0) or dt.itemsize * nc
    raw = np.frombuffer(bn, np.uint8, st * (a["count"] - 1) + dt.itemsize * nc, bv.get("byteOffset", 0) + a.get("byteOffset", 0))
    idx = np.arange(a["count"])[:, None] * st + np.arange(dt.itemsize * nc)[None]
    v = raw[idx].copy().view(dt).reshape(-1, nc).astype(np.float64)
    if a.get("normalized"): v /= {5123: 65535.0, 5121: 255.0}[a["componentType"]]
    return v


def check(path, stray):
    js, bn = load(path)
    nodes = js["nodes"]
    bad = 0
    for si, skin in enumerate(js.get("skins", [])):
        inv = acc(js, bn, skin["inverseBindMatrices"]).reshape(-1, 4, 4).transpose(0, 2, 1)   # column-major
        bind = np.linalg.inv(inv)
        head = bind[:, :3, 3]
        # a joint's bone = the segments to each of its child joints (a branching joint like the chest has several);
        # a leaf is a point
        jidx = {n: k for k, n in enumerate(skin["joints"])}
        segs = []
        for k, n in enumerate(skin["joints"]):
            ch = [jidx[c] for c in nodes[n].get("children", []) if c in jidx]
            segs.append([head[c] for c in ch] or [head[k]])
        for ni, nd in enumerate(nodes):
            if nd.get("skin") != si or nd.get("mesh") is None: continue
            mesh = js["meshes"][nd["mesh"]]
            for pi, pr in enumerate(mesh["primitives"]):
                at = pr["attributes"]
                if "JOINTS_0" not in at: continue
                P = acc(js, bn, at["POSITION"]); J = acc(js, bn, at["JOINTS_0"]).astype(int); W = acc(js, bn, at["WEIGHTS_0"])
                s = W.sum(1)
                unnorm = int((np.abs(s - 1) > 2e-3).sum())
                extra = "JOINTS_1" in at
                # distance of each weighted vertex to the nearest segment of its bone
                d = np.full(J.shape, np.inf)
                for c in range(max(len(x) for x in segs)):
                    tail = np.array([x[min(c, len(x) - 1)] for x in segs])
                    a, b = head[J], tail[J]
                    ab = b - a; t = np.clip(((P[:, None] - a) * ab).sum(-1) / np.maximum((ab * ab).sum(-1), 1e-12), 0, 1)
                    d = np.minimum(d, np.linalg.norm(P[:, None] - (a + ab * t[..., None]), axis=-1))
                far = (W > 0.05) & (d > stray)
                nfar = int(far.any(1).sum())
                tag = "OK " if not (unnorm or extra or nfar) else "BAD"
                if tag == "BAD": bad += 1
                worst = ""
                if nfar:
                    v, k = np.unravel_index(np.argmax(np.where(far, d, 0)), d.shape)
                    worst = f" worst: {nodes[skin['joints'][J[v, k]]]['name']} w={W[v, k]:.2f} at {d[v, k]:.2f} m"
                print(f"  {tag} {mesh.get('name')}[{pi}] {len(P)} verts: sum!=1 {unnorm}, JOINTS_1 {extra}, stray {nfar}{worst}")
    return bad


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    stray = 0.25
    for a in sys.argv[1:]:
        if a.startswith("--stray="): stray = float(a.split("=")[1])
    total = 0
    for p in args:
        print(p)
        total += check(p, stray)
    print("primitives with problems:", total)
    sys.exit(1 if total else 0)
