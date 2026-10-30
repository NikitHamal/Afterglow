"""Lengthen a card-based hair style in place: a long fall from an existing short style.

The hair styles are card strips skinned 100% to 頭, and their atlas tiles vertically (the cap's UVs run past -3), so a
card can simply keep going. Vertices below `--from` are moved down with a smooth ramp, which stretches the strands and
the atlas bands together - a bob becomes a straight fall to `--to` without new geometry or a Blender session.

    python tools/model/hair/stretch_style.py <in.glb> <out.glb> --from 1.60 --to 1.26 [--spread 1.05] [--back 0.01]
"""
import argparse
import json
import struct

import numpy as np


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
    return js, bytearray(bn)


def acc_bounds(js, i):
    a = js["accessors"][i]
    return a["count"], a["min"], a["max"]


def stretch(js, bn, y_from, y_to, spread, back):
    done = 0
    for mesh in js["meshes"]:
        for prim in mesh["primitives"]:
            a = js["accessors"][prim["attributes"]["POSITION"]]
            bv = js["bufferViews"][a["bufferView"]]
            o = bv.get("byteOffset", 0) + a.get("byteOffset", 0)
            n = a["count"]
            P = np.frombuffer(bytes(bn), np.float32, n * 3, o).reshape(-1, 3).astype(np.float64).copy()
            y = P[:, 1]
            y_min = float(y.min())
            k = np.clip((y_from - y) / max(1e-4, y_from - y_min), 0, 1)
            k = k * k * (3 - 2 * k)
            P[:, 1] = y + k * (y_to - y_min)
            P[:, 0] *= spread
            P[:, 2] -= k * back
            bn[o:o + 12 * n] = P.astype(np.float32).tobytes()
            a["min"] = [float(P[:, i].min()) for i in range(3)]
            a["max"] = [float(P[:, i].max()) for i in range(3)]
            done += n
    return done


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("src")
    ap.add_argument("dst")
    ap.add_argument("--from", dest="y_from", type=float, required=True)
    ap.add_argument("--to", dest="y_to", type=float, required=True)
    ap.add_argument("--spread", type=float, default=1.04)
    ap.add_argument("--back", type=float, default=0.012)
    a = ap.parse_args()
    js, bn = read_glb(a.src)
    n = stretch(js, bn, a.y_from, a.y_to, a.spread, a.back)
    js["meshes"][0]["name"] = "hair_long_kari"
    js["materials"][0]["name"] = "hair_long_kari"
    j = json.dumps(js, separators=(",", ":")).encode()
    j += b" " * ((4 - len(j) % 4) % 4)
    blob = bytes(bn)
    blob += b"\0" * ((4 - len(blob) % 4) % 4)
    total = 12 + 8 + len(j) + 8 + len(blob)
    with open(a.dst, "wb") as f:
        f.write(struct.pack("<III", 0x46546C67, 2, total))
        f.write(struct.pack("<II", len(j), 0x4E4F534A) + j)
        f.write(struct.pack("<II", len(blob), 0x004E4942) + blob)
    print("stretched", n, "vertices ->", a.dst)


if __name__ == "__main__":
    main()
