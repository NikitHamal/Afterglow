# Hair kit I/O: the strand atlas + cap texture, the hair-style GLB writer (characters/hair/<style>.glb) and the splice
# that replaces the hair embedded in a character GLB (old data compacted away, nothing else touched).
import json, struct, io, math
import numpy as np
from PIL import Image
from hairkit import ss, HEAD, HAIR_LIN, to_gltf

# ---------------------------------------------------------------------------------------------- textures
def strand_atlas(rng, size=2048, ncol=8):
    """8 columns, V root (0) -> tip (1) in rows before the flip: 0-2 dense sheets, 3 separated locks, 4 waves,
    5 fine wisps (baby hairs, flyaways), 6 coils (natural curls), 7 short strands (a man's top)"""
    TH, TW = size, size; CW = TW // ncol
    lum = np.zeros((TH, TW), np.float32); alp = np.zeros((TH, TW), np.float32)
    v = np.linspace(0, 1, TH)
    def strands(c, n, base, lum_rng, wav_amp, w_rng, end_rng, freq=(0.6, 2.2), curl=0.0, root_fade=0.015):
        sl = slice(c * CW, (c + 1) * CW)
        if base > 0:
            u = np.linspace(0, 1, CW)
            b = (ss(0.08, 0.40, u) * ss(0.92, 0.60, u))[None, :] * (base * ss(0.0, 0.03, v) * ss(1.0, 0.86, v))[:, None]
            alp[:, sl] = np.maximum(alp[:, sl], b); lum[:, sl] = np.where(b > 0, 0.6, lum[:, sl])
        for _ in range(n):
            x0 = float(np.clip(CW / 2 + rng.normal(0, CW * 0.22), 8, CW - 8)); wpx = rng.uniform(*w_rng); end = rng.uniform(*end_rng)
            L0 = rng.uniform(*lum_rng); fq = rng.uniform(*freq); ph = rng.uniform(0, 6.3); drift = rng.uniform(-5, 5)
            start = rng.uniform(0.0, 0.35) if curl > 0 else 0.0
            xs = x0 + wav_amp * rng.uniform(0.3, 1.0) * np.sin(2 * np.pi * fq * v + ph) + drift * v
            if curl > 0:   # coils: tight loops, short visible runs
                xs = x0 + curl * np.sin(2 * np.pi * rng.uniform(9, 16) * v + ph) * rng.uniform(0.6, 1.0)
            a_len = ss(start, start + root_fade * rng.uniform(0.6, 1.4), v) * ss(end, end - 0.16, v) * rng.uniform(0.85, 1.0)
            shade = L0 * (0.55 + 0.45 * ss(0.0, 0.30, v)) * (1.0 + 0.16 * ss(0.55, 1.0, v))
            for dxp in range(-3, 4):
                xi = np.clip(np.round(xs).astype(int) + dxp, 0, CW - 1)
                cov = np.clip(1 - np.abs(xi - xs) / wpx, 0, 1) * a_len
                gx = c * CW + xi; rows = np.arange(TH)
                a_old = alp[rows, gx]; a_new = a_old + cov * (1 - a_old)
                lum[rows, gx] = np.where(a_new > 1e-4, (lum[rows, gx] * a_old + shade * cov * (1 - a_old)) / np.maximum(a_new, 1e-4), lum[rows, gx])
                alp[rows, gx] = a_new
    for c in range(3): strands(c, 380, 0.55, (0.66, 1.0), 1.5, (0.9, 1.5), (0.82, 1.0))
    strands(3, 200, 0.25, (0.66, 1.02), 2.5, (0.9, 1.4), (0.66, 1.0))
    strands(4, 170, 0.20, (0.66, 1.02), 7.0, (0.9, 1.4), (0.70, 1.0), freq=(1.5, 2.5))
    # soft-rooted wisps: hairline fringes and flyaways fade in out of the skin over the first third
    strands(5, 120, 0.0, (0.75, 1.0), 3.0, (0.7, 1.1), (0.6, 1.0), root_fade=0.32)
    strands(6, 320, 0.45, (0.45, 1.0), 0.0, (1.0, 1.6), (0.75, 1.0), curl=CW * 0.13)
    strands(7, 420, 0.5, (0.5, 1.0), 1.0, (1.0, 1.6), (0.45, 0.95))
    a8 = np.clip(alp * 1.1, 0, 1); a8 = np.where(a8 < 0.08, 0.0, a8)
    rgb = np.stack([lum, lum * 0.93, lum * 0.86], -1)
    img = np.concatenate([rgb, a8[..., None]], -1)
    return Image.fromarray(np.flipud(np.clip(img * 255 + 0.5, 0, 255).astype(np.uint8)), "RGBA")

def cap_texture(rng, size=512, coily=False):
    # the cap is shaded as the hair's under-layer (x0.5 in the shader): painted light so the gaps match the cards
    CT = size; cl = np.full((CT, CT), 0.85, np.float32); vv = np.arange(CT) / CT
    for _ in range(900 if not coily else 1300):
        x0 = rng.uniform(0, CT); L0 = rng.uniform(0.72, 1.0); fq = rng.integers(1, 3); ph = rng.uniform(0, 6.3)
        amp = 2.0 if not coily else rng.uniform(3, 6)
        f = fq if not coily else rng.integers(18, 30)
        xs = (x0 + amp * np.sin(2 * np.pi * f * vv + ph)) % CT
        for dxp in (-1, 0, 1):
            xi = (np.round(xs).astype(int) + dxp) % CT
            cov = np.clip(1 - np.abs(((xi - xs + CT / 2) % CT) - CT / 2) / 1.2, 0, 1) * 0.7
            cl[np.arange(CT), xi] = cl[np.arange(CT), xi] * (1 - cov) + L0 * cov
    return Image.fromarray(np.clip(np.stack([cl, cl * 0.93, cl * 0.86], -1) * 255 + 0.5, 0, 255).astype(np.uint8), "RGB")

def png_bytes(im):
    b = io.BytesIO(); im.save(b, "PNG", optimize=True); return b.getvalue()

# ---------------------------------------------------------------------------------------------- vertex data
def skin_arrays(prim, joint_names):
    """weights dicts -> JOINTS_0 (indices into joint_names) / WEIGHTS_0 (top 4, normalised)"""
    n = len(prim["w"]); J = np.zeros((n, 4), np.uint16); W = np.zeros((n, 4), np.float32)
    jidx = {nm: i for i, nm in enumerate(joint_names)}
    for i, wd in enumerate(prim["w"]):
        items = sorted(((v, k) for k, v in wd.items() if v > 1e-4), reverse=True)[:4] or [(1.0, HEAD)]
        s = sum(v for v, _ in items)
        for c, (v, k) in enumerate(items): J[i, c] = jidx.get(k, jidx[HEAD]); W[i, c] = v / s
    return J, W

class Buf:
    def __init__(self, js, bn):
        self.js, self.bn = js, bn
    def add(self, data, ctype, atype, target=None, minmax=False, normalized=False):
        js, bn = self.js, self.bn
        while len(bn) % 4: bn.append(0)
        raw = np.ascontiguousarray(data).tobytes()
        bv = {"buffer": 0, "byteOffset": len(bn), "byteLength": len(raw)}
        if target: bv["target"] = target
        bn.extend(raw); js["bufferViews"].append(bv)
        cnt = int(data.shape[0])
        a = {"bufferView": len(js["bufferViews"]) - 1, "componentType": ctype, "count": cnt, "type": atype}
        if normalized: a["normalized"] = True
        if minmax:
            a["min"] = [float(x) for x in data.min(0)]; a["max"] = [float(x) for x in data.max(0)]
        js["accessors"].append(a); return len(js["accessors"]) - 1
    def image(self, png, name):
        js, bn = self.js, self.bn
        while len(bn) % 4: bn.append(0)
        js["bufferViews"].append({"buffer": 0, "byteOffset": len(bn), "byteLength": len(png)}); bn.extend(png)
        js["images"].append({"name": name, "mimeType": "image/png", "bufferView": len(js["bufferViews"]) - 1})
        return len(js["images"]) - 1

F32, U16, U32, U8 = 5126, 5123, 5125, 5121
def prim_accessors(B, prim, joint_names, morphs):
    pos = to_gltf(prim["pos"]).astype(np.float32); nr = to_gltf(prim["nrm"]).astype(np.float32)
    J, W = skin_arrays(prim, joint_names)
    col = np.concatenate([np.full((len(pos), 3), 255), np.round(prim["alpha"][:, None] * 255)], 1).astype(np.uint8)
    n = len(pos)
    # byte-packed where it loses nothing: coverage, joint indices (< 256), weights (the engine renormalises them)
    Wb = np.round(W * 255).astype(np.int32)
    Wb[:, 0] += 255 - Wb.sum(1)
    attrs = {"POSITION": B.add(pos, F32, "VEC3", 34962, minmax=True), "NORMAL": B.add(nr, F32, "VEC3", 34962),
             "TEXCOORD_0": B.add(prim["uv"].astype(np.float32), F32, "VEC2", 34962),
             "COLOR_0": B.add(col, U8, "VEC4", 34962, normalized=True),
             "JOINTS_0": B.add(J.astype(np.uint8), U8, "VEC4", 34962) if J.max() < 256 else B.add(J, U16, "VEC4", 34962),
             "WEIGHTS_0": B.add(np.clip(Wb, 0, 255).astype(np.uint8), U8, "VEC4", 34962, normalized=True)}
    idx = prim["tris"].reshape(-1).astype(np.uint32 if n > 65535 else np.uint16)
    out = {"attributes": attrs, "indices": B.add(idx, U32 if n > 65535 else U16, "SCALAR", 34963)}
    if morphs:
        out["targets"] = [{"POSITION": B.add(to_gltf(d).astype(np.float32), F32, "VEC3", 34962, minmax=True)} for d in morphs]
    return out

HAIR_ROUGH = 0.74   # broad, soft sheen: cards at grazing angles stay hair-coloured instead of flashing grey
def material(name, tex, cutoff=0.42, factor=HAIR_LIN):
    return {"name": name, "alphaMode": "MASK", "alphaCutoff": cutoff, "doubleSided": True,
            "pbrMetallicRoughness": {"baseColorFactor": [*factor, 1.0], "baseColorTexture": {"index": tex},
                                     "metallicFactor": 0.0, "roughnessFactor": HAIR_ROUGH}}

def write_style_glb(path, style, prims, morph_names, atlas_png, cap_png, joint_names):
    """a hair-style GLB: joints (names only, the engine binds them to the character's skeleton), one mesh"""
    js = {"asset": {"version": "2.0", "generator": "afterglow hairkit"}, "scene": 0, "scenes": [{"nodes": [0]}],
          "nodes": [], "meshes": [], "skins": [], "materials": [], "textures": [], "images": [], "samplers": [{}],
          "accessors": [], "bufferViews": [], "buffers": [{"byteLength": 0}]}
    bn = bytearray(); B = Buf(js, bn)
    js["nodes"].append({"name": f"hair_{style}_rig", "children": list(range(2, 2 + len(joint_names)))})
    js["nodes"].append({"name": f"hair_{style}", "mesh": 0, "skin": 0})
    js["scenes"][0]["nodes"] = [0, 1]
    for nm in joint_names: js["nodes"].append({"name": nm})
    ib = np.tile(np.eye(4, dtype=np.float32).reshape(1, 16), (len(joint_names), 1))
    js["skins"].append({"joints": list(range(2, 2 + len(joint_names))), "inverseBindMatrices": B.add(ib, F32, "MAT4")})
    ia = B.image(atlas_png, f"hair_{style}"); ic = B.image(cap_png, f"hair_{style}_cap")
    js["textures"] = [{"source": ia, "sampler": 0}, {"source": ic, "sampler": 0}]
    js["materials"] = [material(f"hair_{style}", 0), material(f"hair_{style}_cap", 1)]
    mesh = {"name": f"hair_{style}", "primitives": [], "extras": {"targetNames": list(morph_names)}}
    for mi, p in enumerate(prims):
        if p is None: continue
        pa = prim_accessors(B, p, joint_names, p.get("morphs"))
        pa["material"] = mi; mesh["primitives"].append(pa)
    js["meshes"].append(mesh)
    write_glb(path, js, bn)

# ---------------------------------------------------------------------------------------------- GLB file
def read_glb(path):
    b = open(path, "rb").read(); off, js, bn = 12, None, None
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
    with open(path, "wb") as f:
        f.write(struct.pack("<III", 0x46546C67, 2, 12 + 8 + len(j) + 8 + len(bn)))
        f.write(struct.pack("<II", len(j), 0x4E4F534A)); f.write(j)
        f.write(struct.pack("<II", len(bn), 0x004E4942)); f.write(bytes(bn))

def compact(js, bn):
    """drop buffer views nothing references any more (spliced-out data), re-pack the BIN"""
    used = set()
    for a in js["accessors"]:
        if "bufferView" in a: used.add(a["bufferView"])
        if "sparse" in a: used.add(a["sparse"]["indices"]["bufferView"]); used.add(a["sparse"]["values"]["bufferView"])
    for im in js.get("images", []):
        if "bufferView" in im: used.add(im["bufferView"])
    order = sorted(used); remap = {}; nb = bytearray(); views = []
    for old in order:
        bv = dict(js["bufferViews"][old]); o, L = bv.get("byteOffset", 0), bv["byteLength"]
        while len(nb) % 4: nb.append(0)
        bv["byteOffset"] = len(nb); nb.extend(bn[o:o + L]); remap[old] = len(views); views.append(bv)
    js["bufferViews"] = views
    for a in js["accessors"]:
        if "bufferView" in a: a["bufferView"] = remap[a["bufferView"]]
        if "sparse" in a:
            a["sparse"]["indices"]["bufferView"] = remap[a["sparse"]["indices"]["bufferView"]]
            a["sparse"]["values"]["bufferView"] = remap[a["sparse"]["values"]["bufferView"]]
    for im in js.get("images", []):
        if "bufferView" in im: im["bufferView"] = remap[im["bufferView"]]
    # accessors nothing references are harmless; drop them too so the file stays tidy
    return js, nb

def splice_hair(src, dst, mesh_name, prims, morph_names, atlas_png, cap_png):
    """replace a character GLB's hair mesh (cards + cap primitives) and its two hair images"""
    js, bn = read_glb(src)
    skin = js["skins"][0]
    joint_names = [js["nodes"][j]["name"] for j in skin["joints"]]
    mi = next(i for i, m in enumerate(js["meshes"]) if m.get("name") == mesh_name)
    mesh = js["meshes"][mi]
    old = mesh["primitives"]
    B = Buf(js, bn)
    newp = []
    for k, p in enumerate(prims):
        if p is None: continue
        pa = prim_accessors(B, p, joint_names, p.get("morphs"))
        pa["material"] = old[min(k, len(old) - 1)]["material"]
        newp.append(pa)
    mesh["primitives"] = newp
    mesh.setdefault("extras", {})["targetNames"] = list(morph_names)
    # the hair materials' images: replaced in place (same image slots); the same soft sheen as the styles
    for pa, png in zip(newp, (atlas_png, cap_png)):
        mat = js["materials"][pa["material"]]
        mat["pbrMetallicRoughness"]["roughnessFactor"] = HAIR_ROUGH
        ti = mat["pbrMetallicRoughness"]["baseColorTexture"]["index"]
        im = js["images"][js["textures"][ti]["source"]]
        while len(bn) % 4: bn.append(0)
        js["bufferViews"].append({"buffer": 0, "byteOffset": len(bn), "byteLength": len(png)}); bn.extend(png)
        im["bufferView"] = len(js["bufferViews"]) - 1; im["mimeType"] = "image/png"
    # drop accessors only the old primitives used (keep indices stable: rebuild the accessor list)
    used_acc = set()
    def mark(a):
        if a is not None: used_acc.add(a)
    for m in js["meshes"]:
        for p in m["primitives"]:
            for a in p["attributes"].values(): mark(a)
            mark(p.get("indices"))
            for t in p.get("targets", []):
                for a in t.values(): mark(a)
    for s in js.get("skins", []): mark(s.get("inverseBindMatrices"))
    for an in js.get("animations", []):
        for smp in an["samplers"]: mark(smp["input"]); mark(smp["output"])
    order = sorted(used_acc); amap = {o: n for n, o in enumerate(order)}
    js["accessors"] = [js["accessors"][o] for o in order]
    for m in js["meshes"]:
        for p in m["primitives"]:
            p["attributes"] = {k: amap[v] for k, v in p["attributes"].items()}
            if "indices" in p: p["indices"] = amap[p["indices"]]
            if "targets" in p: p["targets"] = [{k: amap[v] for k, v in t.items()} for t in p["targets"]]
    for s in js.get("skins", []):
        if "inverseBindMatrices" in s: s["inverseBindMatrices"] = amap[s["inverseBindMatrices"]]
    for an in js.get("animations", []):
        for smp in an["samplers"]: smp["input"] = amap[smp["input"]]; smp["output"] = amap[smp["output"]]
    js, bn = compact(js, bn)
    write_glb(dst, js, bn)
    return joint_names
