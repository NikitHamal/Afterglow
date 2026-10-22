# Marcus stage C: realistic skin in three tones (honey / caramel / deep) for the 4K body texture and the 1K genital texture.
# Stubble beard, chest / trail / pubic / forearm / leg hair, male areolae, natural nails, scalp under the hair,
# shaft veins + glans + circumcision line, scrotal rugae + raphe. Shared normal + ORM per texture.
#   blender -b out/marcus_b.blend --python male_paint.py       (REUSE=1 reuses cached bakes)
import bpy, os, math, json, time, shlex
import numpy as np
from mathutils import Vector
from PIL import Image
M = "/root/agb/male"; OUT = M + "/out"; TEX = M + "/tex"; os.makedirs(TEX, exist_ok=True)
SK = "/root/agb/mh_assets/skins"
SRC = {"cau": SK + "/middleage_caucasian_male/" + [f for f in os.listdir(SK + "/middleage_caucasian_male") if f.endswith(".png")][0],
       "afr": SK + "/middleage_african_male/" + [f for f in os.listdir(SK + "/middleage_african_male") if f.endswith(".png")][0]}
RES, GRES = 4096, 1024
def prog(*a): os.system("python3 /root/agb/progress.py " + " ".join(shlex.quote(str(x)) for x in a))
def log(*a): print("[male]", *a, flush=True); prog("log", "[male] " + " ".join(str(x) for x in a))
t0 = time.time()
body = bpy.data.objects["Human"]; rig = bpy.data.objects["Human.rig"]
G = json.load(open(OUT + "/genital.json")); B, A, D = (np.array(G[k]) for k in ("B", "A", "D"))
bones = {b.name: np.array(rig.matrix_world @ b.head_local) for b in rig.data.bones}
for kb in body.data.shape_keys.key_blocks: kb.value = 0.0
def vgw(name):
    w = np.zeros(len(body.data.vertices), np.float32)
    if name not in body.vertex_groups: return w
    gi = body.vertex_groups[name].index
    for v in body.data.vertices:
        for g in v.groups:
            if g.group == gi: w[v.index] = g.weight
    return w
sc = bpy.context.scene
sc.render.engine = "CYCLES"; sc.cycles.device = "CPU"; sc.cycles.samples = 1; sc.cycles.use_denoising = False
for o in bpy.context.view_layer.objects: o.select_set(False)
body.select_set(True); bpy.context.view_layer.objects.active = body
def add_attr(values, attr):
    a = body.data.color_attributes.get(attr) or body.data.color_attributes.new(attr, "FLOAT_COLOR", "POINT")
    c = np.zeros((len(values), 4), np.float32); c[:, :3] = values[:, None]; c[:, 3] = 1; a.data.foreach_set("color", c.ravel())
add_attr(vgw("fingernails"), "m_nails"); add_attr(vgw("head"), "m_head")
def bake(name, build, btype="EMIT", samples=1, margin=16):
    imgs = [bpy.data.images.new(name + "_b", RES, RES, alpha=True, float_buffer=True), bpy.data.images.new(name + "_g", GRES, GRES, alpha=True, float_buffer=True)]
    saved = [s.material for s in body.material_slots]
    for si, img in enumerate(imgs):
        img.colorspace_settings.name = "Non-Color"
        mat = bpy.data.materials.new(f"BK_{name}_{si}"); mat.use_nodes = True; nt = mat.node_tree; nt.nodes.clear()
        out = nt.nodes.new("ShaderNodeOutputMaterial")
        if btype == "EMIT":
            em = nt.nodes.new("ShaderNodeEmission"); nt.links.new(em.outputs[0], out.inputs[0]); nt.links.new(build(nt), em.inputs["Color"])
        else:
            bs = nt.nodes.new("ShaderNodeBsdfDiffuse"); nt.links.new(bs.outputs[0], out.inputs[0])
        tx = nt.nodes.new("ShaderNodeTexImage"); tx.image = img; nt.nodes.active = tx
        body.material_slots[si].material = mat
    sc.cycles.samples = samples; sc.render.bake.margin = margin; sc.render.bake.margin_type = "EXTEND"
    t = time.time(); bpy.ops.object.bake(type=btype, use_clear=True); print(f"baked {name} {time.time()-t:.1f}s", flush=True)
    for s, m in zip(body.material_slots, saved): s.material = m
    res = []
    for img, r in zip(imgs, (RES, GRES)):
        a = np.empty(r * r * 4, np.float32); img.pixels.foreach_get(a); res.append(a.reshape(r, r, 4)); bpy.data.images.remove(img)
    return res
def geo(sock): return lambda nt: nt.nodes.new("ShaderNodeNewGeometry").outputs[sock]
def attr(name):
    def f(nt):
        a = nt.nodes.new("ShaderNodeAttribute"); a.attribute_name = name; return a.outputs["Color"]
    return f
def one(nt):
    r = nt.nodes.new("ShaderNodeRGB"); r.outputs[0].default_value = (1, 1, 1, 1); return r.outputs[0]
def proc(nt):
    tc = nt.nodes.new("ShaderNodeTexCoord").outputs["Object"]
    def vor(s):
        v = nt.nodes.new("ShaderNodeTexVoronoi"); v.inputs["Scale"].default_value = s; nt.links.new(tc, v.inputs["Vector"]); return v.outputs["Distance"]
    def noi(s, d):
        n = nt.nodes.new("ShaderNodeTexNoise"); n.inputs["Scale"].default_value = s; n.inputs["Detail"].default_value = d; nt.links.new(tc, n.inputs["Vector"]); return n.outputs["Fac"]
    cmb = nt.nodes.new("ShaderNodeCombineColor")
    nt.links.new(vor(900), cmb.inputs[0]); nt.links.new(noi(380, 8), cmb.inputs[1]); nt.links.new(noi(22, 4), cmb.inputs[2])
    return cmb.outputs[0]
CACHE = OUT + "/bakes.npz"
KEYS = ("COV", "POS", "NRM", "MN", "MH", "HP", "AO")
if os.environ.get("REUSE") and os.path.exists(CACHE):
    z = np.load(CACHE); BK = {k: z[k] for k in KEYS}; GK = {k: z["g" + k] for k in KEYS}; log("bakes from cache")
else:
    BK, GK = {}, {}
    for k, fn, bt, smp in (("COV", one, "EMIT", 1), ("POS", geo("Position"), "EMIT", 1), ("NRM", geo("Normal"), "EMIT", 1),
                           ("MN", attr("m_nails"), "EMIT", 1), ("MH", attr("m_head"), "EMIT", 1), ("HP", proc, "EMIT", 1), ("AO", None, "AO", 48)):
        b_, g_ = bake(k, fn, bt, smp)
        BK[k], GK[k] = b_, g_
    for d_ in (BK, GK):
        d_["COV"] = d_["COV"][..., 0] > 0.5; d_["POS"] = d_["POS"][..., :3]; d_["NRM"] = d_["NRM"][..., :3]
        for k in ("MN", "MH", "AO"): d_[k] = d_[k][..., 0]
        d_["HP"] = d_["HP"][..., :3]
    np.savez(CACHE, **BK, **{"g" + k: v for k, v in GK.items()}); log(f"bakes done ({time.time()-t0:.0f}s)")

def ss(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0, 1); return t * t * (3 - 2 * t)
def lerp3(a, b, t): return a + (b - a) * t[..., None]
def to_lin(c): return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)
def to_srgb(c): c = np.clip(c, 0, 1); return np.where(c <= 0.0031308, c * 12.92, 1.055 * c ** (1 / 2.4) - 0.055)
POS, NRM, COV, HP, AO, MH, MN = (BK[k] for k in ("POS", "NRM", "COV", "HP", "AO", "MH", "MN"))
Z = lambda: np.zeros(POS.shape[:2], np.float32)
EC = (bones["head"] + np.array([0, -0.09, 0.07]))                           # rough eye-line centre
Lm = {}
# landmarks from the mesh
V = np.array([v.co for v in body.data.vertices])
eyes = [o for o in bpy.data.objects if "high-poly" in o.name][0]
E = np.array([eyes.matrix_world @ v.co for v in eyes.data.vertices]); EL, ER = E[E[:, 0] > 0].mean(0), E[E[:, 0] < 0].mean(0); EC = (EL + ER) / 2
wh = vgw("head"); headv = V[wh > 0.5]
CHIN_Z = headv[(np.abs(headv[:, 0]) < 0.015) & (headv[:, 1] < EC[1])][:, 2].min()
NIP = []
for s_ in (1, -1):
    c = V[(np.sign(V[:, 0]) == s_) & (np.abs(V[:, 2] - (bones["spine_03"][2] + 0.02)) < 0.09) & (np.abs(V[:, 0]) > 0.06) & (np.abs(V[:, 0]) < 0.14)]
    NIP.append(c[np.argmin(c[:, 1])])
NAVEL_Z = bones["spine_01"][2] + 0.03
log(f"landmarks eyes {EC.round(3)} chin {CHIN_Z:.3f} nipples {NIP[0].round(3)}")

# ------------------------------------------------------------------ body masks
Mk = {k: Z() for k in ("stubble", "chest", "trail", "pubic", "arm", "leg", "pit", "areola", "flush", "scalp", "lipm", "nail", "brow")}
phi = np.arctan2(POS[..., 0] - EC[0], -(POS[..., 1] - (EC[1] + 0.075)))
face = ss(0.3, 0.7, MH) * COV
# stubble: jaw, chin, upper lip, lower cheeks; fades onto the neck
jawline = CHIN_Z + 0.012 + 0.045 * (np.abs(phi) / 1.6) ** 1.5
upper = EC[2] - 0.045 - 0.02 * ss(0.2, 0.9, np.abs(phi))
beard = ss(upper + 0.006, upper - 0.008, POS[..., 2]) * ss(1.85, 1.6, np.abs(phi))
neck = ss(CHIN_Z - 0.045, CHIN_Z - 0.01, POS[..., 2]) * (POS[..., 1] < EC[1] + 0.06)
Mk["stubble"] = (beard * face + neck * (POS[..., 2] < CHIN_Z + 0.01) * COV).astype(np.float32)
HCEN = np.array([0.0, EC[1] + 0.07, EC[2] - 0.02])
outward = ss(0.15, 0.45, np.sum(NRM * (POS - HCEN), -1) / np.maximum(1e-6, np.linalg.norm(POS - HCEN, axis=-1)))
Mk["stubble"] *= outward                                                  # skin only, never the inner mouth
Mk["stubble"] *= 1 - ss(0.012, 0.004, np.linalg.norm(POS - (EC + np.array([0, -0.06, -0.08])), axis=-1)) * 0  # (lips handled below)
mouth_c = EC + np.array([0.0, -0.035, -0.083])
dm = np.linalg.norm((POS - mouth_c) * np.array([1, 1, 1.6]), axis=-1)
Mk["lipm"] = ss(0.016, 0.011, dm) * face * (POS[..., 1] < mouth_c[1] + 0.012)
Mk["stubble"] *= 1 - Mk["lipm"]
# chest hair: sternum + around nipples, sparse
cx = POS[..., 0]; cz = POS[..., 2]
chest_c = np.array([0.0, NIP[0][1], NIP[0][2] + 0.02])
Mk["chest"] = (np.exp(-((cx / 0.075) ** 2 + ((cz - chest_c[2]) / 0.09) ** 2)) * (NRM[..., 1] < -0.3) * COV).astype(np.float32)
for n_ in NIP: Mk["chest"] = np.maximum(Mk["chest"], np.exp(-np.sum((POS - n_) ** 2, -1) / (2 * 0.035 ** 2)) * 0.6 * COV)
# happy trail: navel to pubis
Mk["trail"] = (ss(0.022, 0.006, np.abs(cx)) * ss(NAVEL_Z + 0.06, NAVEL_Z, cz) * ss(B[2] + 0.02, B[2] + 0.05, cz) * (NRM[..., 1] < -0.3) * COV).astype(np.float32)
pub = np.exp(-((cx / 0.062) ** 2 + ((cz - (B[2] + 0.04)) / 0.038) ** 2)) * (NRM[..., 1] < -0.2) * (cz > B[2] - 0.005)
Mk["pubic"] = (np.clip(pub * 1.6, 0, 1) * COV).astype(np.float32)
for s_ in ("l", "r"):
    e0, h0 = bones[f"lowerarm_{s_}"], bones[f"hand_{s_}"]
    ab = h0 - e0; t = np.clip(((POS - e0) @ ab) / (ab @ ab), 0, 1); dd = np.linalg.norm(POS - (e0 + t[..., None] * ab), axis=-1)
    Mk["arm"] = np.maximum(Mk["arm"], ss(0.05, 0.03, dd) * ss(0.0, 0.25, t) * ss(1.0, 0.8, t) * COV)
    k0, f0 = bones[f"calf_{s_}"], bones[f"foot_{s_}"]
    ab = f0 - k0; t = np.clip(((POS - k0) @ ab) / (ab @ ab), 0, 1); dd = np.linalg.norm(POS - (k0 + t[..., None] * ab), axis=-1)
    Mk["leg"] = np.maximum(Mk["leg"], ss(0.075, 0.05, dd) * ss(0.95, 0.75, t) * COV)
    t0_, k1 = bones[f"thigh_{s_}"], bones[f"calf_{s_}"]
    ab = k1 - t0_; t = np.clip(((POS - t0_) @ ab) / (ab @ ab), 0, 1); dd = np.linalg.norm(POS - (t0_ + t[..., None] * ab), axis=-1)
    Mk["leg"] = np.maximum(Mk["leg"], 0.55 * ss(0.11, 0.08, dd) * ss(0.25, 0.6, t) * COV)
    pit = bones[f"upperarm_{s_}"] + np.array([0, 0.01, -0.06])
    Mk["pit"] = np.maximum(Mk["pit"], np.exp(-np.sum((POS - pit) ** 2, -1) / (2 * 0.022 ** 2)) * COV)
for n_ in NIP:
    d_ = np.linalg.norm(POS - n_, axis=-1); Mk["areola"] = np.maximum(Mk["areola"], ss(0.0135, 0.009, d_) * COV)
for bn, r, s_ in [("calf_l", 0.05, 0.2), ("calf_r", 0.05, 0.2), ("lowerarm_l", 0.035, 0.15), ("lowerarm_r", 0.035, 0.15)]:
    c = bones[bn]; Mk["flush"] = np.maximum(Mk["flush"], np.exp(-np.sum((POS - c) ** 2, -1) / r ** 2) * s_ * COV)
HLA = [0.0, 0.35, 0.70, 1.0, 1.25, 1.45, 1.75, 2.05, 2.5, math.pi]
HLZ = [0.074, 0.070, 0.060, 0.040, 0.026, 0.018, 0.004, -0.04, -0.07, -0.078]
hl = EC[2] + np.interp(np.abs(phi), HLA, HLZ)
Mk["scalp"] = ss(-0.004, 0.006, POS[..., 2] - hl) * ss(0.3, 0.7, MH) * COV
Mk["nail"] = ss(0.3, 0.75, MN) * COV
strand = ss(0.45, 0.75, HP[..., 1]) * 0.6 + ss(0.08, 0.02, HP[..., 0]) * 0.6   # hair-dot noise
log("body masks ready: " + ", ".join(f"{k}={int((v > 0.5).sum())}" for k, v in Mk.items()))
Image.fromarray(np.flipud(np.clip(np.stack([Mk["stubble"], Mk["pubic"] + Mk["chest"], Mk["scalp"]], -1) * 255, 0, 255).astype(np.uint8))).resize((1024, 1024)).save(OUT + "/dbg_masks.jpg")

# ------------------------------------------------------------------ genital masks (UV space: shaft u<0.62, sack u>0.64)
gu = np.linspace(0, 1, GRES)[None, :].repeat(GRES, 0); gv = np.linspace(0, 1, GRES)[:, None].repeat(GRES, 1)   # row 0 = v 0
GCOV = GK["COV"]; GHP = GK["HP"]
is_sack = (gu > 0.63) & GCOV; is_shaft = (gu < 0.625) & GCOV
L_s = 0.215; sc_n = (L_s - 0.039) / L_s                         # corona station in normalized length
glans = is_shaft * ss(sc_n - 0.004, sc_n + 0.004, gv)
corona = is_shaft * np.exp(-((gv - (sc_n + 0.012)) / 0.012) ** 2)
scar = is_shaft * np.exp(-((gv - (sc_n - 0.07)) / 0.010) ** 2)
ang = (gu / 0.62) * 2 * np.pi                                     # 0 = ventral seam
vein = np.zeros_like(gu)
rng = np.random.default_rng(3)
for k in range(4):                                                # dorsal veins wandering along the shaft
    a0 = math.pi + rng.uniform(-0.9, 0.9); amp = rng.uniform(0.15, 0.35); fq = rng.uniform(1.5, 3.5); ph = rng.uniform(0, 6.3)
    path = a0 + amp * np.sin(fq * gv * 2 * np.pi + ph)
    w_ = rng.uniform(0.045, 0.07)
    vein = np.maximum(vein, np.exp(-((ang - path) / w_) ** 2) * ss(0.05, 0.2, gv) * ss(sc_n - 0.05, sc_n - 0.15, gv) * rng.uniform(0.6, 1.0))
vein *= is_shaft
ridge = is_shaft * np.exp(-(np.minimum(ang, 2 * np.pi - ang) / 0.35) ** 2) * (1 - glans)       # ventral raphe on the shaft
meatus = is_shaft * ss(0.985, 0.995, gv) * np.exp(-(np.minimum(ang, 2 * np.pi - ang) / 0.5) ** 2)
su = (gu - 0.64) / 0.36; sang_ = su * 2 * np.pi
raphe = is_sack * np.exp(-(np.minimum(np.abs(sang_ - math.pi / 2), np.abs(sang_ - 1.5 * math.pi)) / 0.06) ** 2)
warp = GHP[..., 1] * 9 + GHP[..., 2] * 5
rugae = is_sack * (0.5 + 0.25 * np.sin(gv * 70 + 2.2 * np.sin(sang_ * 2 + warp) + warp) + 0.25 * np.sin(gv * 37 - sang_ * 5 + warp * 1.7))
log("genital masks ready")

# ------------------------------------------------------------------ tones
def load_src(k, r): return to_lin(np.flipud(np.asarray(Image.open(SRC[k]).convert("RGB").resize((r, r), Image.LANCZOS), np.float32) / 255.0))
bodym = COV & (MH < 0.2) & (MN < 0.2)
TONES = {"honey": dict(mix=(1.0, 0.0), mean=(188, 128, 95), lip=(0.62, 0.36, 0.33), hair=0.62),
         "caramel": dict(mix=(0.45, 0.55), mean=(148, 96, 66), lip=(0.50, 0.28, 0.25), hair=0.55),
         "deep": dict(mix=(0.0, 1.0), mean=(95, 59, 41), lip=(0.34, 0.19, 0.17), hair=0.45)}
SRCL = {k: load_src(k, RES) for k in SRC}
HAIRC = to_lin(np.array([0.11, 0.075, 0.06]))
for tone, T in TONES.items():
    C = SRCL["cau"] * T["mix"][0] + SRCL["afr"] * T["mix"][1]
    C = C * (to_lin(np.array(T["mean"]) / 255) / C[bodym].mean(0))
    cheek = (face > 0.5) & (Mk["stubble"] < 0.05) & (POS[..., 2] < EC[2]) & (POS[..., 2] > EC[2] - 0.05) & (np.abs(phi) < 1.2)
    beardz = (Mk["stubble"] > 0.6) & (face > 0.5)
    if cheek.any() and beardz.any():                                      # neutralise the source texture's grey beard zone
        C = lerp3(C, C * (C[cheek].mean(0) / C[beardz].mean(0)), np.clip(Mk["stubble"] * 1.2, 0, 1))
    C *= (1 + (HP[..., 2:3] - 0.5) * np.array([0.06, 0.0, -0.03])); C *= (0.82 + 0.18 * AO)[..., None]
    C = lerp3(C, C * np.array([0.93, 0.80, 0.78]) ** 2.2, Mk["flush"])
    C = lerp3(C, C * np.array([0.62, 0.50, 0.46]) ** 2.2, Mk["areola"] * 0.8)
    hairamt = T["hair"]
    for k, dens in (("stubble", 0.5), ("chest", 0.5), ("trail", 0.55), ("pubic", 0.9), ("arm", 0.34), ("leg", 0.32), ("pit", 0.75)):
        m = Mk[k] * dens * (0.35 + 0.65 * strand) * (hairamt / 0.55)
        C = lerp3(C, C * 0.35 + HAIRC * 0.65, np.clip(m, 0, 0.95))
    C = lerp3(C, C * np.array([0.86, 0.83, 0.85]) ** 2.2, Mk["stubble"] * 0.4)           # faint bluish shadow of the beard
    C = lerp3(C, to_lin(np.array(T["lip"])) * np.clip((C.mean(-1, keepdims=True) / max(1e-4, C[Mk["lipm"] > 0.5].mean())) ** 0.4, 0.7, 1.3), Mk["lipm"] * 0.55)
    C = lerp3(C, C * np.array([1.05, 0.95, 0.95]), Mk["nail"] * 0.5)
    C = lerp3(C, HAIRC * (0.6 + 0.4 * strand[..., None]), Mk["scalp"] * 0.92)
    Image.fromarray(np.flipud(np.clip(to_srgb(C) * 255 + 0.5, 0, 255).astype(np.uint8))).save(f"{TEX}/marcus_skin_{tone}.jpg", quality=90)
    # genital texture matched to the pubic skin of this tone
    near = bodym & (np.linalg.norm(POS - (B + np.array([0, 0.01, 0.03])), axis=-1) < 0.05) & (Mk["pubic"] < 0.3)
    base = C[near].mean(0) if near.any() else C[bodym].mean(0)
    Gc = np.ones((GRES, GRES, 3), np.float32) * base * np.array([0.97, 0.93, 0.92]) ** 2.2
    Gc *= (0.9 + 0.2 * GHP[..., 2:3]); Gc *= (0.8 + 0.2 * GK["AO"])[..., None]
    Gc = lerp3(Gc, Gc * np.array([0.84, 0.78, 0.88]) ** 2.2, vein * 0.22)
    Gc = lerp3(Gc, Gc * np.array([0.88, 0.80, 0.80]) ** 2.2, ridge * 0.4)
    lum = float(base.mean()) / 0.13
    glc = 0.45 * base * np.array([0.95, 0.82, 0.86]) ** 2.2 + 0.55 * to_lin(np.array([0.60, 0.40, 0.43])) * min(1.0, 0.45 + 0.55 * lum)   # dusky rose
    Gc = lerp3(Gc, glc * (0.92 + 0.16 * GHP[..., 1:2]), glans)
    Gc = lerp3(Gc, Gc * np.array([0.8, 0.62, 0.66]) ** 2.2, corona * 0.5)
    Gc = lerp3(Gc, Gc * np.array([0.85, 0.75, 0.75]) ** 2.2, scar * 0.5)
    Gc = lerp3(Gc, np.full(3, 0.02), meatus * 0.9)
    sack = base * np.array([0.80, 0.68, 0.66]) ** 2.2
    Gc = lerp3(Gc, sack * (0.88 + 0.16 * rugae[..., None]), is_sack.astype(np.float32))
    Gc = lerp3(Gc, np.ones_like(Gc) * base, (is_shaft * ss(0.07, 0.0, gv) + is_sack * ss(0.12, 0.0, gv)).astype(np.float32))   # melt into the body skin at the base
    Gc = lerp3(Gc, Gc * 0.6, raphe * 0.6)
    Gc = lerp3(Gc, Gc * 0.35 + HAIRC * 0.65, (is_sack * ss(0.55, 0.85, GHP[..., 1]) * 0.35 * ss(0.4, 0.1, gv)).astype(np.float32))
    Gc = lerp3(Gc, Gc * 0.35 + HAIRC * 0.65, (is_shaft * ss(0.12, 0.0, gv) * 0.5 * ss(0.5, 0.8, GHP[..., 1])).astype(np.float32))
    Image.fromarray(np.flipud(np.clip(to_srgb(Gc) * 255 + 0.5, 0, 255).astype(np.uint8))).save(f"{TEX}/marcus_genital_{tone}.jpg", quality=90)
    log(f"tone {tone} painted")
del SRCL

# ------------------------------------------------------------------ normal + ORM (body, genital)
def nmap(h, k=0.9):
    dx = np.zeros_like(h); dy = np.zeros_like(h)
    dx[:, 1:-1] = (h[:, 2:] - h[:, :-2]) * 0.5; dy[1:-1, :] = (h[2:, :] - h[:-2, :]) * 0.5
    n = np.stack([-dx * k, -dy * k, np.ones_like(h)], -1); return n / np.linalg.norm(n, axis=-1, keepdims=True)
pores = face * ss(0.4, 0.1, HP[..., 0])
hairy = np.clip(Mk["stubble"] + Mk["chest"] * 0.5 + Mk["pubic"] + Mk["scalp"], 0, 1)
h = -0.4 * pores + (HP[..., 1] - 0.5) * (0.6 + 0.3 * face + 0.12 * hairy) + (HP[..., 2] - 0.5) * 0.25
R = 0.55 + (HP[..., 1] - 0.5) * 0.06 + 0.05 * pores - 0.12 * Mk["lipm"] + 0.2 * hairy - 0.15 * Mk["nail"]
def save(a, path, q):
    Image.fromarray(np.flipud(np.clip(a * 255 + 0.5, 0, 255).astype(np.uint8))).save(path, quality=q)
save(nmap(h) * 0.5 + 0.5, TEX + "/marcus_skin_normal.jpg", 92)
save(np.stack([np.clip(0.25 + 0.75 * AO, 0, 1), np.clip(R, 0.15, 0.9), np.zeros_like(R)], -1), TEX + "/marcus_skin_orm.jpg", 90)
gh = 1.2 * vein + 0.6 * ridge + 0.6 * rugae * is_sack - 0.9 * raphe + 0.8 * corona - 1.5 * meatus + (GHP[..., 1] - 0.5) * 0.4
GR = np.where(glans > 0.5, 0.36, 0.52) + 0.08 * is_sack
save(nmap(gh, 1.4) * 0.5 + 0.5, TEX + "/marcus_genital_normal.jpg", 92)
save(np.stack([np.clip(0.3 + 0.7 * GK["AO"], 0, 1), np.clip(GR, 0.2, 0.9), np.zeros_like(GR)], -1), TEX + "/marcus_genital_orm.jpg", 90)
log(f"normal + ORM maps ({time.time()-t0:.0f}s)")

# ------------------------------------------------------------------ materials
def timg(nt, path, cs):
    n = nt.nodes.new("ShaderNodeTexImage"); n.image = bpy.data.images.load(path); n.image.colorspace_settings.name = cs; return n
def pbr(name, base, normal, orm, ss_w=0.18):
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; nt.nodes.clear()
    o_ = nt.nodes.new("ShaderNodeOutputMaterial"); bs = nt.nodes.new("ShaderNodeBsdfPrincipled"); nt.links.new(bs.outputs[0], o_.inputs[0])
    b_ = timg(nt, base, "sRGB"); b_.name = "BASE"; nt.links.new(b_.outputs[0], bs.inputs["Base Color"])
    n_ = timg(nt, normal, "Non-Color"); nm = nt.nodes.new("ShaderNodeNormalMap"); nt.links.new(n_.outputs[0], nm.inputs["Color"]); nt.links.new(nm.outputs[0], bs.inputs["Normal"])
    r_ = timg(nt, orm, "Non-Color"); sp = nt.nodes.new("ShaderNodeSeparateColor"); nt.links.new(r_.outputs[0], sp.inputs[0])
    nt.links.new(sp.outputs[1], bs.inputs["Roughness"]); nt.links.new(sp.outputs[2], bs.inputs["Metallic"])
    g = bpy.data.node_groups.get("glTF Material Output") or bpy.data.node_groups.new("glTF Material Output", "ShaderNodeTree")
    if not g.interface.items_tree: g.interface.new_socket("Occlusion", in_out="INPUT", socket_type="NodeSocketFloat")
    gn = nt.nodes.new("ShaderNodeGroup"); gn.node_tree = g; nt.links.new(sp.outputs[0], gn.inputs["Occlusion"])
    bs.inputs["Subsurface Weight"].default_value = ss_w; bs.inputs["Subsurface Radius"].default_value = (1.0, 0.38, 0.2); bs.inputs["Subsurface Scale"].default_value = 0.01
    return m
body.material_slots[0].material = pbr("marcus_skin", TEX + "/marcus_skin_caramel.jpg", TEX + "/marcus_skin_normal.jpg", TEX + "/marcus_skin_orm.jpg")
body.material_slots[1].material = pbr("marcus_genital", TEX + "/marcus_genital_caramel.jpg", TEX + "/marcus_genital_normal.jpg", TEX + "/marcus_genital_orm.jpg", 0.25)
def tex_mat(name, img_path, rough, tint=None, alpha=False):
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; nt.nodes.clear()
    o_ = nt.nodes.new("ShaderNodeOutputMaterial"); bs = nt.nodes.new("ShaderNodeBsdfPrincipled"); nt.links.new(bs.outputs[0], o_.inputs[0])
    ti = timg(nt, img_path, "sRGB")
    if tint:
        mx = nt.nodes.new("ShaderNodeMix"); mx.data_type = "RGBA"; mx.blend_type = "MULTIPLY"; mx.inputs["Factor"].default_value = 1
        mx.inputs[7].default_value = (*tint, 1); nt.links.new(ti.outputs[0], mx.inputs[6]); nt.links.new(mx.outputs[2], bs.inputs["Base Color"])
    else: nt.links.new(ti.outputs[0], bs.inputs["Base Color"])
    if alpha: nt.links.new(ti.outputs[1], bs.inputs["Alpha"]); m.blend_method = "HASHED"
    bs.inputs["Roughness"].default_value = rough
    return m
def part(sub): return [o for o in bpy.data.objects if o.type == "MESH" and sub in o.name][0]
EY = "/root/agb/mh_assets/eyes/materials/"
be = np.asarray(Image.open(EY + "brown_eye.png").convert("RGBA"), np.float32); bl = np.asarray(Image.open(EY + "brownlight_eye.png").convert("RGBA"), np.float32)
ey = be.copy(); ey[..., :3] = np.clip((0.6 * be[..., :3] + 0.4 * bl[..., :3]) * np.array([0.82, 0.74, 0.66]), 0, 255)
Image.fromarray(ey.astype(np.uint8), "RGBA").save(TEX + "/marcus_eye.png")
br = Image.open("/root/agb/mh_assets/eyebrows/eyebrow001/eyebrow001.png").convert("RGBA"); ba = np.asarray(br, np.float32)
nz = np.asarray(Image.effect_noise(br.size, 64).convert("L"), np.float32) / 255
rgb = np.array([30, 21, 16], np.float32)[None, None] * (0.75 + 0.5 * nz[..., None])
Image.fromarray(np.dstack([np.clip(rgb, 0, 255), ba[..., 3:]]).astype(np.uint8), "RGBA").save(TEX + "/marcus_brow.png")
def first_img(o):
    for nd in o.material_slots[0].material.node_tree.nodes:
        if nd.type == "TEX_IMAGE" and nd.image: return bpy.path.abspath(nd.image.filepath)
part("high-poly").material_slots[0].material = tex_mat("marcus_eye", TEX + "/marcus_eye.png", 0.04, alpha=True)
part("eyebrow").material_slots[0].material = tex_mat("marcus_brow", TEX + "/marcus_brow.png", 0.6, alpha=True)
o = part("eyelashes"); o.material_slots[0].material = tex_mat("marcus_lash", first_img(o), 0.5, alpha=True)
o = part("teeth"); o.material_slots[0].material = tex_mat("marcus_teeth", first_img(o), 0.28, tint=(0.85, 0.82, 0.74))
o = part("tongue"); o.material_slots[0].material = tex_mat("marcus_tongue", first_img(o), 0.35, tint=(0.95, 0.82, 0.82))
json.dump({"EL": EL.tolist(), "ER": ER.tolist(), "CHIN_Z": float(CHIN_Z), "NIP": [n.tolist() for n in NIP]}, open(OUT + "/landmarks.json", "w"))
bpy.ops.wm.save_as_mainfile(filepath=OUT + "/marcus_c.blend")
log(f"stage C saved ({time.time()-t0:.0f}s)")
