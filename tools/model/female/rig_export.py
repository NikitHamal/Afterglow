# Vivian -> Afterglow: MMD-named skeleton with Goat-chan's bone frames, weight transfer, cleanup, scale, GLB export.
#   blender -b out/vivian_s5.blend --python rig_export.py
import bpy, bmesh, os, math, json, time, shlex
import numpy as np
from mathutils import Vector, Matrix

OUT = "/root/agb/out"; EXP = OUT + "/export"; PRE = OUT + "/preview"
for d in (EXP, PRE): os.makedirs(d, exist_ok=True)
GAME_H = float(os.environ.get("AG_HEIGHT", "1.35"))   # web: 1.35 incl. hair; native: real body height with AG_BODY_ONLY=1
def prog(*a): os.system("python3 /root/agb/progress.py " + " ".join(shlex.quote(str(x)) for x in a))
def log(*a): print("[rig]", *a, flush=True); prog("log", " ".join(str(x) for x in a))
t0 = time.time()
sc = bpy.context.scene
body = bpy.data.objects["Human"]; mrig = bpy.data.objects["Human.rig"]
meshes = [o for o in sc.objects if o.type == "MESH"]
for o in meshes:
    if o.data.shape_keys:
        for kb in o.data.shape_keys.key_blocks: kb.value = 0.0

# ------------------------------------------------------------------ 1. drop MakeHuman helper geometry (keeps shape keys)
wb_idx = body.vertex_groups["body"].index
bm = bmesh.new(); bm.from_mesh(body.data)
dl = bm.verts.layers.deform.active
kill = [v for v in bm.verts if v[dl].get(wb_idx, 0.0) < 0.5]
bmesh.ops.delete(bm, geom=kill, context="VERTS"); bm.to_mesh(body.data); bm.free(); body.data.update()
for m in list(body.modifiers):
    if m.type == "MASK": body.modifiers.remove(m)
log(f"helper geometry removed: body now {len(body.data.vertices)} verts")

# ------------------------------------------------------------------ 2. MPFB joint positions (world)
J = {b.name: (mrig.matrix_world @ b.head_local, mrig.matrix_world @ b.tail_local) for b in mrig.data.bones}
zs = [(o.matrix_world @ v.co).z for o in meshes for v in o.data.vertices]
if os.environ.get("AG_BODY_ONLY"):
    zs = [(body.matrix_world @ v.co).z for v in body.data.vertices]
top = max(zs); S = GAME_H / top
log(f"model height {top:.3f} m (incl. hair) -> scale {S:.4f}")
goat = {b["n"]: b for b in json.load(open("/root/agb/goat_bones.json"))}

# ------------------------------------------------------------------ 3. nails -> finger palm directions (curl axis)
wn = np.zeros(len(body.data.vertices), np.float32); gi = body.vertex_groups["fingernails"].index
for v in body.data.vertices:
    for g in v.groups:
        if g.group == gi: wn[v.index] = g.weight
NV = np.array([body.matrix_world @ v.co for v in body.data.vertices])[wn > 0.5]
def seg_dist(P, a, b):
    a, b = np.array(a), np.array(b); ab = b - a; t = np.clip(((P - a) @ ab) / (ab @ ab), 0, 1)
    return np.linalg.norm(P - (a + t[:, None] * ab), axis=1), a + t[:, None] * ab
PALM = {}
for side in "lr":
    for f in ("thumb", "index", "middle", "ring", "pinky"):
        h3, t3 = J[f"{f}_03_{side}"]
        d, foot = seg_dist(NV, h3, t3)
        sel = d < 0.012
        if sel.sum() < 3: continue
        dorsal = (NV[sel] - foot[sel]).mean(0); dorsal /= np.linalg.norm(dorsal)
        PALM[f + side] = -dorsal
log(f"palm directions from nails: {len(PALM)} fingers")

# ------------------------------------------------------------------ 4. build the MMD armature
LR = {"l": ".L", "r": ".R"}
FING = {"thumb": "親指", "index": "人指", "middle": "中指", "ring": "薬指", "pinky": "小指"}
def P(name, end=0): return Vector(J[name][end]) * S
spec = []        # (name, head, tail, parent, roll_mode)
hips = (P("thigh_l") + P("thigh_r")) / 2
waist = P("spine_01")
spec += [("全ての親", Vector((0, 0, 0)), Vector((0, 0, 0.08)), None, "goat"),
         ("センター", Vector((0, 0, hips.z * 0.73)), Vector((0, 0, hips.z * 0.70)), "全ての親", "goat"),
         ("グルーブ", Vector((0, 0, hips.z * 0.75)), Vector((0, 0, hips.z * 0.98)), "センター", "goat"),
         ("腰", Vector((0, hips.y, hips.z)), waist.copy(), "グルーブ", "goat"),
         ("上半身", waist.copy(), P("spine_02"), "腰", "goat"),
         ("上半身2", P("spine_02"), P("neck_01"), "上半身", "goat"),
         ("首", P("neck_01"), P("head"), "上半身2", "goat"),
         ("頭", P("head"), P("head") + Vector((0, 0, 0.09)), "首", "goat")]
gd = Vector(goat["下半身"]["t"]) - Vector(goat["下半身"]["h"]); gd.normalize()
spec.append(("下半身", waist.copy(), waist + gd * 0.075, "腰", "goat"))
for s_, L in LR.items():
    spec += [(f"肩{L}", P(f"clavicle_{s_}"), P(f"upperarm_{s_}"), "上半身2", "goat"),
             (f"腕{L}", P(f"upperarm_{s_}"), P(f"lowerarm_{s_}"), f"肩{L}", "goat"),
             (f"ひじ{L}", P(f"lowerarm_{s_}"), P(f"hand_{s_}"), f"腕{L}", "goat"),
             (f"手首{L}", P(f"hand_{s_}"), P(f"middle_01_{s_}"), f"ひじ{L}", "goat"),
             (f"足{L}", P(f"thigh_{s_}"), P(f"calf_{s_}"), "下半身", "goat"),
             (f"ひざ{L}", P(f"calf_{s_}"), P(f"foot_{s_}"), f"足{L}", "goat"),
             (f"足首{L}", P(f"foot_{s_}"), P(f"ball_{s_}"), f"ひざ{L}", "goat"),
             (f"つま先{L}", P(f"ball_{s_}"), P(f"ball_{s_}", 1), f"足首{L}", "goat"),
             (f"乳親{L}", P(f"breast_{s_}"), P(f"breast_{s_}", 1), "上半身2", "goat")]
    for f, jp in FING.items():
        segs = [P(f"{f}_01_{s_}"), P(f"{f}_02_{s_}"), P(f"{f}_03_{s_}"), P(f"{f}_03_{s_}", 1)]
        n = 3 if f == "thumb" else 4
        if n == 4:
            tipdir = (segs[3] - segs[2]).normalized(); segs.append(segs[3] + tipdir * (segs[3] - segs[2]).length * 0.6)
        parent = f"手首{L}"
        for k in range(n):
            nm = f"{jp}{'０１２３'[k]}{L}"
            spec.append((nm, segs[k], segs[k + 1], parent, ("palm", f + s_)))
            parent = nm

# hair physics chains (hair.py writes the joints): 髪.<n>.a-d + unweighted tip leaf 髪.<n>.z, rooted on 頭
HAIR_SEGS = "abcd"
if os.path.exists(OUT + "/hair_chains.json"):
    for n_, J in enumerate(json.load(open(OUT + "/hair_chains.json"))):
        J = [Vector(j) * S for j in J]
        parent = "頭"
        for k in range(4):
            spec.append((f"髪.{n_}.{HAIR_SEGS[k]}", J[k], J[k + 1], parent, "none")); parent = f"髪.{n_}.{HAIR_SEGS[k]}"
        tipd = (J[4] - J[3]).normalized()
        spec.append((f"髪.{n_}.z", J[4], J[4] + tipd * 0.02, parent, "none"))
# eye bones at the eyeball centres, pointing forward (gaze is driven in game)
eyes_o = next((o for o in meshes if "high-poly" in o.name), None)
if eyes_o:
    EV = np.array([eyes_o.matrix_world @ v.co for v in eyes_o.data.vertices])
    for sgn, L in ((1, ".L"), (-1, ".R")):
        cE = Vector(EV[np.sign(EV[:, 0]) == sgn].mean(0)) * S
        spec.append((f"目{L}", cE, cE + Vector((0, -0.02, 0)), "頭", "none"))
arm_data = bpy.data.armatures.new("VivianRig"); arm = bpy.data.objects.new("VivianRig", arm_data)
sc.collection.objects.link(arm); bpy.context.view_layer.objects.active = arm
for o in bpy.context.view_layer.objects: o.select_set(False)
arm.select_set(True); bpy.ops.object.mode_set(mode="EDIT")
eb = arm_data.edit_bones
for name, h, t, parent, roll in spec:
    b = eb.new(name); b.head = h; b.tail = t if (t - h).length > 1e-4 else h + Vector((0, 0, 0.02))
    if parent: b.parent = eb[parent]
    b.use_connect = False
    y = (b.tail - b.head).normalized()
    if roll == "goat":
        g = goat.get(name)
        if g is None: continue
        xg = Vector(g["x"]); x = (xg - y * xg.dot(y))
        if x.length < 1e-4: continue
        x.normalize(); b.align_roll(x.cross(y))           # z = x × y
    elif isinstance(roll, tuple) and roll[1] in PALM:
        z = Vector(PALM[roll[1]]); z = z - y * z.dot(y)
        if z.length > 1e-4: b.align_roll(z.normalized())
bpy.ops.object.mode_set(mode="OBJECT")
log(f"MMD skeleton: {len(arm_data.bones)} bones")

# ------------------------------------------------------------------ 5. weights: MPFB groups -> MMD names
REN = {"pelvis": "下半身", "spine_01": "上半身", "spine_02": "上半身2", "spine_03": "上半身2", "neck_01": "首", "head": "頭"}
for s_, L in LR.items():
    REN.update({f"clavicle_{s_}": f"肩{L}", f"upperarm_{s_}": f"腕{L}", f"lowerarm_{s_}": f"ひじ{L}", f"hand_{s_}": f"手首{L}",
                f"thigh_{s_}": f"足{L}", f"calf_{s_}": f"ひざ{L}", f"foot_{s_}": f"足首{L}", f"ball_{s_}": f"つま先{L}",
                f"breast_{s_}": f"乳親{L}"})
    for f, jp in FING.items():
        for k in range(3): REN[f"{f}_0{k+1}_{s_}"] = f"{jp}{'０１２３'[k]}{L}"
for n_ in range(64):
    for k in HAIR_SEGS: REN[f"hair_{n_}_{k}"] = f"髪.{n_}.{k}"
bone_names = set(arm_data.bones.keys())
for o in meshes:
    old = {vg.index: vg.name for vg in o.vertex_groups}
    acc = {}
    for v in o.data.vertices:
        for g in v.groups:
            nn = REN.get(old.get(g.group))
            if nn and g.weight > 1e-4: acc.setdefault(nn, {}); acc[nn][v.index] = acc[nn].get(v.index, 0.0) + g.weight
    for vg in list(o.vertex_groups): o.vertex_groups.remove(vg)
    for nn, d in acc.items():
        vg = o.vertex_groups.new(name=nn)
        for vi, w in d.items(): vg.add([vi], min(1.0, w), "REPLACE")
    for m in list(o.modifiers):
        if m.type == "ARMATURE": o.modifiers.remove(m)
    unknown = set(acc) - bone_names
    if unknown: log(f"WARN {o.name}: groups without bones {unknown}")
if eyes_o and "目.L" in bone_names:
    for vg in list(eyes_o.vertex_groups): eyes_o.vertex_groups.remove(vg)
    gl, gr = eyes_o.vertex_groups.new(name="目.L"), eyes_o.vertex_groups.new(name="目.R")
    for v in eyes_o.data.vertices: (gl if (eyes_o.matrix_world @ v.co).x > 0 else gr).add([v.index], 1.0, "REPLACE")
log("weights transferred (hair chains + eye bones included)")

# ------------------------------------------------------------------ 6. scale meshes, parent to the new rig, drop MPFB rig
for o in meshes:
    mw = o.matrix_world.copy(); o.parent = None; o.matrix_world = mw
for o in meshes:
    o.select_set(True)
bpy.context.view_layer.objects.active = meshes[0]
for o in meshes:
    o.matrix_world = Matrix.Scale(S, 4) @ o.matrix_world
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
bpy.data.objects.remove(mrig)
for o in meshes:
    o.parent = arm; o.matrix_parent_inverse.identity()
    m = o.modifiers.new("Armature", "ARMATURE"); m.object = arm
    m.use_deform_preserve_volume = True   # dual quaternion, as the engine skins (mesh.vert)
    o.modifiers.move(o.modifiers.find("Armature"), 0)
NAMES = {"Human": "vivian_body", "Human.high-poly": "vivian_eyes", "Human.eyebrow010": "vivian_brows",
         "Human.eyelashes03": "vivian_lashes", "Human.teeth_base": "vivian_teeth", "Human.tongue01": "vivian_tongue", "vivian_hair": "vivian_hair"}
for o in meshes:
    o.name = NAMES.get(o.name, o.name); o.data.name = o.name
log("meshes scaled + bound to the MMD rig: " + ", ".join(o.name for o in meshes))

# ------------------------------------------------------------------ 6b. hair colliders (glTF extras, read by js3d/glbHair.js)
bo = bpy.data.objects["vivian_body"]
BV = np.array([bo.matrix_world @ v.co for v in bo.data.vertices])
def gw(name):
    w = np.zeros(len(BV), np.float32)
    if name not in bo.vertex_groups: return w
    gi = bo.vertex_groups[name].index
    for v in bo.data.vertices:
        for g in v.groups:
            if g.group == gi: w[v.index] = g.weight
    return w
def sphere(P, pct=70, pull=0.0, toward=None):
    c = P.mean(0) if toward is None else P.mean(0) * (1 - pull) + toward * pull
    return c, float(np.percentile(np.linalg.norm(P - c, axis=1), pct))
cols = []
def add(bone, c, r): cols.append({"bone": bone, "c": [float(c[0]), float(c[2]), float(-c[1])], "r": round(r, 4)})
wh = gw("頭"); Ph = BV[wh > 0.9]; ch_ = (Ph.min(0) + Ph.max(0)) / 2; add("頭", ch_, float(np.percentile(np.linalg.norm(Ph - ch_, axis=1), 72)))
wn_ = gw("首"); c, r = sphere(BV[wn_ > 0.5], 60); add("首", c, r)
w2 = gw("上半身2"); sz = np.array(arm.matrix_world @ arm.data.bones["腕.L"].head_local).astype(float)[2]
Pt = BV[(w2 > 0.5) & (BV[:, 2] > sz - 0.10) & (BV[:, 2] < sz + 0.02)]; tc = Pt.mean(0)
for qx in (-1, 1):
    for qy in (-1, 1):
        Q = Pt[(np.sign(Pt[:, 0] - tc[0]) == qx) & (np.sign(Pt[:, 1] - tc[1]) == qy)]
        if len(Q) > 10:
            c, r = sphere(Q, 55, pull=0.45, toward=tc); add("上半身2", c, r * 0.9)
for L in (".L", ".R"):
    wb_ = gw("乳親" + L)
    if (wb_ > 0.5).sum() > 10: c, r = sphere(BV[wb_ > 0.5], 65); add("乳親" + L, c, r)
    wa = gw("肩" + L) + gw("腕" + L); Pa = BV[(wa > 0.5) & (BV[:, 2] > sz - 0.05)]
    if len(Pa) > 10: c, r = sphere(Pa, 55); add("腕" + L, c, r)
# face: two spheres over the cheeks/jaw keep the face-framing tendrils outside the face
Pf = Ph[(Ph[:, 2] < ch_[2] - 0.015) & (Ph[:, 1] < ch_[1] - 0.02)]
for sx_ in (1, -1):
    Q = Pf[np.sign(Pf[:, 0]) == sx_]
    if len(Q) > 20:
        c, r = sphere(Q, 55, pull=0.35, toward=np.array([0.0, Q[:, 1].mean(), Q[:, 2].mean()])); add("頭", c, r)
# no collider may start out containing a hair joint (the solver would fling that strand): shrink to clear it by 6 mm
chain_meta = []
hb = [b for b in arm.data.bones if b.name.startswith("髪.")]
J = np.array([np.array(arm.matrix_world @ b.head_local) for b in hb] + [np.array(arm.matrix_world @ b.tail_local) for b in hb]) if hb else np.zeros((0, 3))
for cdef in cols:
    c = np.array([cdef["c"][0], -cdef["c"][2], cdef["c"][1]])          # back to Blender axes
    if len(J):
        d = float(np.linalg.norm(J - c, axis=1).min())
        if d < cdef["r"] + 0.006: cdef["r"] = round(max(0.01, d - 0.006), 4)
kinds = json.load(open(OUT + "/hair_meta.json"))["kinds"] if os.path.exists(OUT + "/hair_meta.json") else []
for n_, kind in enumerate(kinds):
    # loose hair only: stiff, light gravity, limited swing so it frames the face but never crosses it
    chain_meta.append({"root": f"髪.{n_}.a", "kind": kind, "stiffness": 0.82 if kind == "tendril" else 0.7,
                       "damping": 0.55, "gravity": 0.35, "maxAngleDeg": 28 if kind == "tendril" else 40})
arm["afterglow"] = json.dumps({"hairColliders": cols, "hairChains": chain_meta, "version": 3})
log(f"hair colliders: {len(cols)} spheres, {len(chain_meta)} loose-hair chains -> glTF extras")

# keep a .blend of the game-ready model
bpy.ops.wm.save_as_mainfile(filepath=OUT + "/vivian_game.blend")

# ------------------------------------------------------------------ 7. export GLB (game)
def export(path, anims):
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLB", use_selection=False, export_yup=True, export_apply=False,
                              export_skins=True, export_morph=True, export_morph_normal=True, export_animations=anims,
                              export_image_format="AUTO", export_texcoords=True, export_normals=True, export_tangents=False,
                              export_materials="EXPORT", export_cameras=False, export_lights=False, export_extras=True,
                              export_def_bones=False, export_rest_position_armature=True)
export(EXP + "/vivian_raw.glb", False)
log(f"game GLB exported ({os.path.getsize(EXP + '/vivian_raw.glb')/1e6:.1f} MB raw)")

# ------------------------------------------------------------------ 8. preview GLB with a rig-test clip for the molab viewer
pb = arm.pose.bones
act = bpy.data.actions.new("rig test"); arm.animation_data_create(); arm.animation_data.action = act
FR = 120
def key(bn, f, rx=0, ry=0, rz=0):
    b = pb[bn]; b.rotation_mode = "XYZ"; b.rotation_euler = (rx, ry, rz); b.keyframe_insert("rotation_euler", frame=f)
for f, k in [(1, 0), (30, 1), (60, 0), (90, -1), (120, 0)]:
    key("腕.L", f, 0, 0, 0.5 * k); key("腕.R", f, 0, 0, -0.5 * k)
    key("ひじ.L", f, 0.9 * abs(k)); key("ひじ.R", f, 0.9 * abs(k))
    key("ひざ.L", f, 0.8 * abs(k)); key("ひざ.R", f, 0.4 * abs(k))
    key("足.L", f, -0.5 * abs(k)); key("首", f, 0, 0.35 * k, 0); key("上半身2", f, 0.15 * k)
    for s_ in (".L", ".R"):
        for jp in FING.values():
            for d in "０１２":
                nm = f"{jp}{d}{s_}"
                if nm in pb: key(nm, f, 0.9 * abs(k) if jp != "親指" else 0.5 * abs(k))
bodyo = bpy.data.objects["vivian_body"]
sk = bodyo.data.shape_keys
if sk:
    for kb, frames in [("blink", [(1, 0), (14, 1), (20, 0), (75, 0), (80, 1), (86, 0)]), ("moan", [(1, 0), (30, 1), (60, 0)]),
                       ("smile", [(60, 0), (90, 1), (120, 0)])]:
        for f, v in frames:
            if kb in sk.key_blocks: sk.key_blocks[kb].value = v; sk.key_blocks[kb].keyframe_insert("value", frame=f)
sc.frame_start, sc.frame_end = 1, FR
export(PRE + f"/vivian_preview_{int(time.time())}.glb", True)
for f in sorted(os.listdir(PRE))[:-2]: os.remove(os.path.join(PRE, f))
log(f"preview GLB with rig-test clip exported ({time.time()-t0:.0f}s)")
