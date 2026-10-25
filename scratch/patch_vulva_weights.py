import json
import shutil
import struct
import sys
from pathlib import Path

import numpy as np

CT = {5120: ("b", 1), 5121: ("B", 1), 5122: ("h", 2), 5123: ("H", 2), 5125: ("I", 4), 5126: ("f", 4)}
NC = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}
path = Path(sys.argv[1] if len(sys.argv) > 1 else "/root/Afterglow/native/assets/characters/vivian.glb")
backup = path.with_name("vivian.pre_vulva_fix.glb")
if not backup.exists():
    shutil.copyfile(path, backup)

raw = path.read_bytes()
json_len, json_type = struct.unpack_from("<I4s", raw, 12)
json_raw = raw[20:20 + json_len]
gltf = json.loads(json_raw.decode("utf-8").rstrip("\x00 "))
bin_head = 20 + json_len
bin_len, bin_type = struct.unpack_from("<I4s", raw, bin_head)
bin_start = bin_head + 8
data = bytearray(raw[bin_start:bin_start + bin_len])


def accessor(idx):
    return gltf["accessors"][idx]


def buffer_view(idx):
    return gltf["bufferViews"][idx]


skin = gltf["skins"][0]
joints = skin["joints"]
joint_names = [gltf["nodes"][n].get("name") for n in joints]
pelvis = joint_names.index("下半身")
legs = {joint_names.index(n) for n in ("足.L", "足.R", "ひざ.L", "ひざ.R") if n in joint_names}
legs_names = {joint_names[i] for i in legs}
body = gltf["meshes"][0]
target_names = body.get("extras", {}).get("targetNames", [])
target_idx = target_names.index("vag_open")
retain = 0.12
patched = 0
sum_before = 0.0
sum_after = 0.0

for prim in body["primitives"]:
    if target_idx >= len(prim.get("targets", [])):
        continue
    target = prim["targets"][target_idx]["POSITION"]
    ta = accessor(target)
    if "sparse" in ta:
        sp = ta["sparse"]
        ind = sp["indices"]
        ibv = buffer_view(ind["bufferView"])
        io = ibv.get("byteOffset", 0) + ind.get("byteOffset", 0)
        it = CT[ind["componentType"]][0]
        verts = np.frombuffer(data, dtype=np.dtype(it), count=sp["count"], offset=io).astype(int)
    else:
        fb = accessor(prim["attributes"]["POSITION"])["count"]
        delta = data  # unused fallback
        verts = np.arange(fb)

    ja = accessor(prim["attributes"]["JOINTS_0"])
    wa = accessor(prim["attributes"]["WEIGHTS_0"])
    jbv = buffer_view(ja["bufferView"])
    wbv = buffer_view(wa["bufferView"])
    jstride = jbv.get("byteStride", 4 * CT[ja["componentType"]][1])
    wstride = wbv.get("byteStride", 4 * CT[wa["componentType"]][1])
    jfmt = CT[ja["componentType"]][0]
    wfmt = CT[wa["componentType"]][0]
    jbase = jbv.get("byteOffset", 0) + ja.get("byteOffset", 0)
    wbase = wbv.get("byteOffset", 0) + wa.get("byteOffset", 0)

    for vi in verts:
        jo = jbase + int(vi) * jstride
        wo = wbase + int(vi) * wstride
        jj = list(struct.unpack_from("<4" + jfmt, data, jo))
        ww = list(struct.unpack_from("<4" + wfmt, data, wo))
        entries = {}
        legw = 0.0
        for j, w in zip(jj, ww):
            if w <= 1e-6:
                continue
            entries[j] = entries.get(j, 0.0) + float(w)
            if j in legs:
                legw += float(w)
        if legw < 0.005:
            continue
        sum_before += legw
        rem = 0.0
        for j in list(entries):
            if j in legs:
                keep = entries[j] * retain
                rem += entries[j] - keep
                entries[j] = keep
        entries[pelvis] = entries.get(pelvis, 0.0) + rem
        items = sorted(entries.items(), key=lambda kv: -kv[1])[:4]
        total = sum(w for _, w in items)
        if total <= 1e-6:
            continue
        outj = [j for j, _ in items] + [0] * (4 - len(items))
        outw = [w / total for _, w in items] + [0.0] * (4 - len(items))
        struct.pack_into("<4" + jfmt, data, jo, *outj)
        struct.pack_into("<4" + wfmt, data, wo, *outw)
        patched += 1

out = bytearray(raw)
out[bin_start:bin_start + bin_len] = data
path.write_bytes(out)
print(f"patched {patched} vulva vertices; leg weight before {sum_before:.1f}, retained {retain:.2f}; backup {backup}")
