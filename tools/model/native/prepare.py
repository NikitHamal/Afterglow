# Native prep of a game-ready character blend (output of rig_export.py at real scale):
#   blender -b /root/agb/male/out/marcus_game.blend --python prepare.py -- marcus
#   blender -b /root/agb/out/vivian_game.blend      --python prepare.py -- vivian
# -> /root/agb/native/<who>_native.blend (+ <who>_measure.json)
# Marcus: length/girth up+dn pairs, body morphs carried onto the hair shell, erect-girth measurement.
# Vivian: amplified weak body morphs, vag_open / anal_open (radial opening fields around the canal axes,
#         sized to Marcus' measured erect girth), body morphs carried onto the hair that lies on her torso.
# Both: actions from the preview export removed, materials made safe for a plain PBR renderer.
import bpy, sys, os, json, math
import numpy as np
from mathutils import Vector
from mathutils.kdtree import KDTree

WHO = sys.argv[sys.argv.index("--") + 1]
NAT = "/root/agb/native"; os.makedirs(NAT, exist_ok=True)
def log(*a): print(f"[prep:{WHO}]", *a, flush=True)
body = bpy.data.objects[f"{WHO}_body"]
hair = bpy.data.objects.get(f"{WHO}_hair")
arm = [o for o in bpy.data.objects if o.type == "ARMATURE"][0]
sk = body.data.shape_keys.key_blocks
def co(kb):
    a = np.empty(len(kb.data) * 3, np.float32); kb.data.foreach_get("co", a); return a.reshape(-1, 3)
def put(kb, X): kb.data.foreach_set("co", np.asarray(X, np.float32).ravel())
def attr(name):
    a = body.data.attributes.get(name)
    if a is None: return None
    v = np.empty(len(a.data), np.float32); a.data.foreach_get("value", v); return v
def add_key(obj, name, X):
    if obj.data.shape_keys is None: obj.shape_key_add(name="Basis", from_mix=False)
    kb = obj.data.shape_keys.key_blocks.get(name) or obj.shape_key_add(name=name, from_mix=False)
    put(kb, X); kb.value = 0.0; return kb
B = co(sk["Basis"])
for kb in sk: kb.value = 0.0

# ---------------------------------------------------------------- cleanup: preview actions, NLA
for ob in bpy.data.objects:
    if ob.animation_data: ob.animation_data_clear()
    if ob.type == "MESH" and ob.data.shape_keys and ob.data.shape_keys.animation_data:
        ob.data.shape_keys.animation_data_clear()
for act in list(bpy.data.actions): bpy.data.actions.remove(act)
for pb in arm.pose.bones: pb.matrix_basis.identity()

# ---------------------------------------------------------------- materials: plain-PBR safe
for m in bpy.data.materials:
    if not m.use_nodes: continue
    for n in m.node_tree.nodes:
        if n.type == "BSDF_PRINCIPLED":
            if not n.inputs["Metallic"].is_linked: n.inputs["Metallic"].default_value = 0.0
            if not n.inputs["Roughness"].is_linked:
                n.inputs["Roughness"].default_value = max(0.08, min(1.0, n.inputs["Roughness"].default_value))
            for nm in ("Coat Weight", "Sheen Weight", "Transmission Weight"):
                if nm in n.inputs and not n.inputs[nm].is_linked: n.inputs[nm].default_value = 0.0
        if n.type == "VERTEX_COLOR" or (n.type == "ATTRIBUTE" and n.attribute_name.lower().startswith(("col", "color"))):
            log(f"WARN material {m.name} uses vertex colours ({n.name})")
        if n.type == "TEX_IMAGE" and n.image:
            nm = n.image.name.lower()
            if any(k in nm for k in ("normal", "_n.", "orm", "rough")): n.image.colorspace_settings.name = "Non-Color"
for o in bpy.data.objects:                               # no vertex colour layers ship (GLTF loaders multiply them in)
    if o.type == "MESH" and o != hair:
        for ca in list(o.data.color_attributes):
            try: o.data.color_attributes.remove(ca)
            except RuntimeError: pass

meas = {}
if WHO == "marcus":
    # ------------------------------------------------------------ penis pairs: length/girth up + dn
    for src, up, dn in (("penis_length", "length_up", "length_dn"), ("penis_girth", "girth_up", "girth_dn")):
        if src in sk:
            D = co(sk[src]) - B
            sk[src].name = up
            add_key(body, dn, B - 0.6 * D)
    log("penis pairs:", [k.name for k in sk if k.name.startswith(("length", "girth"))])
    # ------------------------------------------------------------ erect girth (for her opening) + shaft axis
    A = co(sk["aroused"])
    gi = {g.index: g.name for g in body.vertex_groups}
    wshaft = np.zeros(len(B), np.float32)
    for v in body.data.vertices:
        for g in v.groups:
            if gi.get(g.group, "") in ("竿根", "竿中", "竿先"): wshaft[v.index] += g.weight
    r0 = arm.data.bones["竿根"].head_local; r1 = arm.data.bones["竿端"].head_local
    ax = (r1 - r0).normalized(); L = (r1 - r0).length
    P = A[wshaft > 0.6] - np.array(r0); s = P @ np.array(ax)
    rad = np.linalg.norm(P - np.outer(s, ax), axis=1)
    mid = (s > 0.25 * L) & (s < 0.75 * L)
    meas = dict(erect_length=float(L), erect_radius=float(np.median(rad[mid])), erect_radius_max=float(np.percentile(rad[mid], 95)))
    log("erect shaft:", {k: round(v * 1000, 1) for k, v in meas.items()}, "mm")
    CARRY = ["muscle_up", "muscle_dn", "weight_up", "weight_dn", "belly_up"]
else:
    # ------------------------------------------------------------ amplify the weaker body morphs
    AMP = {"butt_up": 2.2, "butt_dn": 1.7, "weight_up": 2.4, "weight_dn": 2.2, "waist_dn": 1.5, "muscle_up": 1.3}
    for nm, k in AMP.items():
        if nm in sk:
            D = co(sk[nm]) - B; put(sk[nm], B + D * k)
            log(f"{nm}: x{k} -> max {np.abs(D * k).max() * 1000:.0f} mm")
    # ------------------------------------------------------------ vag_open / anal_open
    mj = json.load(open(f"{NAT}/marcus_measure.json")) if os.path.exists(f"{NAT}/marcus_measure.json") else {}
    R_vag = float(mj.get("erect_radius_max", 0.0185)) + 0.0008
    cav, anus = attr("v_cav"), attr("v_anus")
    muc, vmin, vmaj = attr("v_muc"), attr("v_min"), attr("v_maj")
    if cav is None:
        log("WARN no v_cav attribute: openings skipped")
    else:
        def opening(sel_canal, sel_surface, R, sigma, name, part=0.0):
            Pc = B[sel_canal]; dc = cav[sel_canal]
            cen = Pc.mean(0); U_, S_, Vt = np.linalg.svd(Pc - cen); a = Vt[0]
            if np.corrcoef((Pc - cen) @ a, dc)[0, 1] < 0: a = -a
            rim = sel_canal & (cav < 0.16)
            E = B[rim].mean(0) if rim.any() else Pc[np.argmin(dc)]
            s_rim = ((B[rim] - E) @ a) if rim.any() else np.zeros(1)
            E = E + a * float(s_rim.min())                 # entrance plane at the outermost rim vertex
            rel = B - E; s = rel @ a; rv = rel - np.outer(s, a); r = np.linalg.norm(rv, axis=1)
            rhat = rv / np.maximum(r, 1e-6)[:, None]
            r_rim = float(np.median(r[rim])) if rim.any() else 0.004
            depth = float(s[sel_canal].max())
            D = np.zeros_like(B)
            # canal walls: open to a smooth tube of radius R (slight taper at the far end)
            prof = R * (1.0 - 0.12 * np.clip(s / max(depth, 1e-4), 0, 1))
            m = sel_canal & (r < prof)
            D[m] = rhat[m] * (prof[m] - r[m])[:, None]
            # surface: everything around the rim is pushed out radially, fading with radius and depth
            near = sel_surface & ~sel_canal
            push = (R - r_rim) * np.exp(-np.maximum(0.0, r - r_rim) ** 2 / (2 * sigma * sigma)) * np.exp(-(np.minimum(s, 0.0) / 0.012) ** 2)
            inside = near & (r < R)                         # tissue lying over the hole (inner labia) goes past the rim
            push = np.where(inside, np.maximum(push, (R - r) * 1.02), push)
            D[near] += rhat[near] * push[near][:, None]
            if part > 0 and vmaj is not None:               # labia majora part laterally as well
                lat = np.sign(B[:, 0]); D[:, 0] += lat * part * vmaj * np.exp(-(np.linalg.norm(rel, axis=1) / 0.03) ** 2)
            add_key(body, name, B + D)
            log(f"{name}: canal depth {depth*1000:.0f} mm, rim r {r_rim*1000:.1f} -> {R*1000:.1f} mm, "
                f"moved {(np.linalg.norm(D, axis=1) > 1e-4).sum()} verts, max {np.linalg.norm(D, axis=1).max()*1000:.1f} mm")
            return dict(entrance=E.tolist(), axis=a.tolist(), depth=depth, radius=R)
        an = anus if anus is not None else np.zeros(len(B))
        vag_canal = (cav > 0.02) & (an < 0.3)
        anal_canal = (cav > 0.02) & (an >= 0.3)
        reg_v = np.zeros(len(B), bool)
        for arr in (muc, vmin, vmaj):
            if arr is not None: reg_v |= arr > 0.02
        reg_v &= an < 0.25
        meas["vagina"] = opening(vag_canal, reg_v | vag_canal, R_vag, 0.011, "vag_open", part=0.004)
        reg_a = (an > 0.03)
        meas["anus"] = opening(anal_canal, reg_a | anal_canal, 0.0115, 0.0075, "anal_open")
    CARRY = ["bust_up", "bust_dn", "weight_up", "weight_dn", "muscle_up"]

# ---------------------------------------------------------------- carry torso morphs onto the hair
if hair is not None:
    deps = bpy.context.evaluated_depsgraph_get()
    Hc = np.array([tuple(v.co) for v in hair.data.vertices], np.float32)
    kd = KDTree(len(B))
    for i, p in enumerate(B): kd.insert(p, i)
    kd.balance()
    idx = np.zeros(len(Hc), np.int64); dist = np.zeros(len(Hc), np.float32)
    for i, p in enumerate(Hc):
        _, j, d = kd.find(p); idx[i] = j; dist[i] = d
    w = np.clip((0.055 - dist) / 0.04, 0, 1)
    made = []
    for nm in CARRY:
        if nm not in sk: continue
        D = (co(sk[nm]) - B)[idx] * w[:, None]
        if np.abs(D).max() < 2e-4: continue
        add_key(hair, nm, Hc + D); made.append(nm)
    log("hair carries:", made)

json.dump(meas, open(f"{NAT}/{WHO}_measure.json", "w"), indent=1)
bpy.ops.wm.save_as_mainfile(filepath=f"{NAT}/{WHO}_native.blend")
log("saved", f"{NAT}/{WHO}_native.blend", "keys:", [k.name for k in body.data.shape_keys.key_blocks][1:])
