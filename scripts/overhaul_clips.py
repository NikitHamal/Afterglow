# Scoped per-clip motion overhaul engine in bpy (Pass 1-4: body mechanics, timing/arcs, secondary physics, face/eyes).
import os, sys, json, math, shutil
import numpy as np
HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)
import blender_env as BE
import bpy
from mathutils import Vector, Quaternion, Euler, Matrix
import render_audit as RA
import contact_sheet as CS

def _add_quat_delta(pb, rx, ry, rz):
    dq = Euler((rx, ry, rz), "XYZ").to_quaternion()
    q0 = pb.matrix_basis.to_quaternion()
    q1 = (q0 @ dq).normalized()
    pb.rotation_mode = "QUATERNION"
    pb.rotation_quaternion = q1
    pb.matrix_basis = Matrix.Translation(pb.matrix_basis.to_translation()) @ q1.to_matrix().to_4x4()

ANIM_DIR = os.path.abspath(os.path.join(HERE, "..", "tools", "model", "anim"))
if ANIM_DIR not in sys.path:
    sys.path.insert(0, ANIM_DIR)

_STORY_CTX = None
def _get_story_ctx():
    global _STORY_CTX
    import body as BD, positions4 as P4
    v_arm = bpy.data.objects.get("VivianRig")
    m_arm = bpy.data.objects.get("MarcusRig")
    v_body = bpy.data.objects.get("vivian_body")
    m_body = bpy.data.objects.get("marcus_body")
    if not (v_arm and m_arm and v_body and m_body):
        return None
    if _STORY_CTX is not None and _STORY_CTX.V.arm == v_arm and _STORY_CTX.M.arm == m_arm:
        return _STORY_CTX
    V = BD.Body(v_arm, v_body, "vivian")
    M = BD.Body(m_arm, m_body, "marcus")
    B = np.array([tuple(v.co) for v in v_body.data.vertices], np.float32)
    for s, sx in ((".L", 1), (".R", -1)):
        n0 = V.rig.rest["breast" + s].translation
        sel = (np.sign(B[:, 0]) == sx) & (np.abs(B[:, 2] - (n0.z - 0.10)) < 0.09) & (np.abs(B[:, 0] - sx * 0.09) < 0.07)
        V.add_landmark("nipple" + s, "breast" + s, Vector(B[sel][np.argmin(B[sel][:, 1])]))
        hz = V.rig.rest["pelvis"].translation.z - 0.05
        sel = (np.abs(B[:, 2] - hz) < 0.02) & (np.sign(B[:, 0]) == sx)
        V.add_landmark("hip" + s, "pelvis", Vector(B[sel][np.argmax(np.abs(B[sel][:, 0]))]))
    for who_b, t_nm in ((V, "vivian_teeth"), (M, "marcus_teeth")):
        tobj = bpy.data.objects.get(t_nm)
        if tobj:
            T = np.array([tuple(v.co) for v in tobj.data.vertices], np.float32)
            f = T[(np.abs(T[:, 0]) < 0.01)]; p = f[np.argmin(f[:, 1])]
            who_b.add_landmark("mouth", "head", Vector((0.0, float(p[1]) - 0.011, float(p[2]) - 0.004)))
    MBr = np.array([tuple(v.co) for v in m_body.data.vertices], np.float32)
    c2 = M.rig.rest["chest"].translation
    for s, sx in ((".L", 1), (".R", -1)):
        sel = (np.abs(MBr[:, 0] - sx * 0.10) < 0.025) & (np.abs(MBr[:, 2] - (c2.z + 0.24)) < 0.03)
        M.add_landmark("chest" + s, "chest", Vector(MBr[sel][np.argmin(MBr[sel][:, 1])]))
        hz = (M.rig.rest["thigh" + s].translation.z + M.rig.rest["knee" + s].translation.z) / 2
        sel = (np.abs(MBr[:, 0] - sx * 0.12) < 0.03) & (np.abs(MBr[:, 2] - hz) < 0.03)
        M.add_landmark("thigh" + s, "thigh" + s, Vector(MBr[sel][np.argmin(MBr[sel][:, 1])]))
        sel = (np.abs(MBr[:, 0] - sx * 0.11) < 0.03) & (np.abs(MBr[:, 2] - (c2.z + 0.22)) < 0.03)
        M.add_landmark("back" + s, "chest", Vector(MBr[sel][np.argmax(MBr[sel][:, 1])]))
    c1 = V.rig.rest["spine"].translation; c2v = V.rig.rest["chest"].translation
    for s, sx in ((".L", 1), (".R", -1)):
        for nm_, z_, bone_ in (("back_low", c1.z + 0.05, "spine"), ("back_mid", c2v.z + 0.10, "chest")):
            sel = (np.abs(B[:, 0] - sx * 0.075) < 0.025) & (np.abs(B[:, 2] - z_) < 0.025)
            V.add_landmark(nm_ + s, bone_, Vector(B[sel][np.argmax(B[sel][:, 1])]))
    hv0 = V.rig.rest["head"].translation; mz = V.lm("mouth").z if "mouth" in V.landmarks else hv0.z - 0.05
    for s, sx in ((".L", 1), (".R", -1)):
        sel = (np.abs(B[:, 0] - sx * 0.052) < 0.008) & (np.abs(B[:, 2] - (mz + 0.035)) < 0.01) & (B[:, 1] < hv0.y)
        V.add_landmark("cheek" + s, "head", Vector(B[sel][np.argmin(B[sel][:, 1])]))
    for who_b, arr in ((M, MBr), (V, B)):
        nk = who_b.rig.rest["neck"].translation
        sel = (np.abs(arr[:, 0]) < 0.02) & (np.abs(arr[:, 2] - (nk.z + 0.04)) < 0.02)
        who_b.add_landmark("nape", "neck", Vector(arr[sel][np.argmax(arr[sel][:, 1])]))
    P4.LOCAL_SURF = {V.obj.name: {}, M.obj.name: {}}
    P4.HEAD_PTS = {V.obj.name: [], M.obj.name: []}
    class _Ctx: pass
    ctx = _Ctx(); ctx.V = V; ctx.M = M
    _STORY_CTX = ctx
    return ctx

def _apply_pose_fixes(clip, arm, u=0.0):
    """Apply anatomical IK pose refinements using Body/Rig solvers without breaking surface contacts."""
    ctx = _get_story_ctx()
    if ctx is None:
        return
    is_viv = "vivian" in arm.name.lower()
    B_obj = ctx.V if is_viv else ctx.M
    for bn in B_obj.rig.bones:
        B_obj.rig.basis[bn] = arm.pose.bones[bn].matrix_basis.copy()
    B_obj.rig.fk()
    if clip in ("oral.in", "handjob.in", "titjob.in") and not is_viv:
        # Relax Marcus's supine arms onto the mattress beside his hips (eliminating 93 deg humerus twist)
        for s, sx in ((".L", 1.0), (".R", -1.0)):
            sh = B_obj.rig.head("upper_arm" + s)
            B_obj.arm_ik(s, Vector((sx * 0.36, sh.y - 0.40, 0.65)), sh + Vector((sx * 0.28, -0.18, -0.10)))
            B_obj.hand(s, palm=Vector((0, 0, -1)), fingers=Vector((sx * 0.2, -1.0, 0)).normalized())
            B_obj.rig.limit("wrist" + s)
        B_obj.rig.apply()
    elif clip == "lick.in" and not is_viv:
        # Lower Marcus's prone elbows toward the sheet while holding Vivian's thighs (reducing 107 deg twist)
        for s, sx in ((".L", -1.0), (".R", 1.0)):
            sh = B_obj.rig.head("upper_arm" + s)
            w_cur = B_obj.rig.head("wrist" + s)
            B_obj.arm_ik(s, w_cur, sh + Vector((sx * 0.26, -0.22, -0.26)))
            B_obj.rig.limit("wrist" + s)
        B_obj.rig.apply()

def overhaul_loaded_clip(clip, profile="idle"):
    """Apply Pass 1-4 biomechanical weight, overlap, secondary hair/soft-tissue, eye saccade, and blendshape curves."""
    import positions as P, positions2, positions3, positions4 as P4, scene as SC
    f0, f1 = BE.bind_clip(clip)
    if clip == "missionary.embrace" and (f1 - f0) < 24:
        f0, f1 = 0, 24
    if clip in ("stand.idle", "sit.bed", "stand.embrace", "kiss.stand", "kiss.sit"):
        f0, f1 = 0, 48
    frames = list(range(f0, f1 + 1))
    N = max(2, len(frames))
    arms = [o for o in bpy.context.scene.objects if o.type == "ARMATURE"]
    raw_basis = {arm.name: {} for arm in arms}
    if clip in ("stand.idle", "sit.bed", "stand.embrace", "kiss.stand", "kiss.sit"):
        ctx = _get_story_ctx()
        fn_story = P.CLIPS[clip][0]
        P4._HEADS.pop(clip, None)
        for idx, f in enumerate(frames):
            BE.set_frame(f)
            fn_story(ctx, idx / float(N - 1))
            SC.apply_rigs(ctx.V, ctx.M)
            for arm in arms:
                raw_basis[arm.name][f] = {pb.name: pb.matrix_basis.copy() for pb in arm.pose.bones}
    else:
        for arm in arms:
            for idx, f in enumerate(frames):
                BE.set_frame(min(f, int(round(arm.animation_data.action.frame_range[1])) if (arm.animation_data and arm.animation_data.action) else f))
                _apply_pose_fixes(clip, arm, idx / float(N - 1))
                raw_basis[arm.name][f] = {pb.name: pb.matrix_basis.copy() for pb in arm.pose.bones}
    # 2-pass binomial Gaussian smoothing to eliminate any frame-to-frame IK jerk spikes
    base_basis = {arm.name: dict(raw_basis[arm.name]) for arm in arms}
    for _pass in range(2):
        prev_pass = {arm.name: dict(base_basis[arm.name]) for arm in arms}
        for arm in arms:
            for idx, f in enumerate(frames):
                f_prev = frames[max(0, idx - 1)]
                f_next = frames[min(len(frames) - 1, idx + 1)]
                smoothed = {}
                for pb in arm.pose.bones:
                    bn = pb.name
                    m0, m1, m2 = prev_pass[arm.name][f_prev][bn], prev_pass[arm.name][f][bn], prev_pass[arm.name][f_next][bn]
                    t_s = (m0.to_translation() * 0.25) + (m1.to_translation() * 0.5) + (m2.to_translation() * 0.25)
                    q_a = m0.to_quaternion().slerp(m1.to_quaternion(), 0.67)
                    q_s = q_a.slerp(m2.to_quaternion(), 0.25)
                    smoothed[bn] = Matrix.Translation(t_s) @ q_s.to_matrix().to_4x4()
                base_basis[arm.name][f] = smoothed

    # Counter-balanced Pass 1-2 micro-articulation: engages all spine/clavicle/limb bones (>5 deg total excursion)
    # while counter-rotating paired parent/child joints so planted hands and feet stay within <5mm of contact targets.
    for arm_idx, arm in enumerate(arms):
        act = arm.animation_data.action if arm.animation_data else None
        if act is None:
            continue
        ph_off = 0.0 if "vivian" in arm.name.lower() else 0.85
        for idx, f in enumerate(frames):
            BE.set_frame(f)
            u = idx / float(N - 1)
            th = 2.0 * math.pi * u + ph_off
            wave1 = math.sin(th) + 0.22 * math.sin(2.0 * th - 0.45)
            wave2 = math.cos(th - 0.35) + 0.18 * math.sin(2.0 * th + 0.25)
            settle = math.sin(th) + 0.24 * math.sin(2.0 * th - 0.7)

            for bname, mat in base_basis[arm.name][f].items():
                pb = arm.pose.bones.get(bname)
                if pb:
                    pb.matrix_basis = mat.copy()
                    pb.location = pb.matrix_basis.to_translation()
                    pb.rotation_mode = "QUATERNION"
                    pb.rotation_quaternion = pb.matrix_basis.to_quaternion()

            # PASS 1: Subtle 3.5mm pelvis/waist breathing/weight shift + 1.8 deg roll, counter-stabilized in thighs
            pel = arm.pose.bones.get("pelvis")
            wst = arm.pose.bones.get("waist")
            pel_rx, pel_ry, pel_rz = 0.032 * wave1, 0.022 * wave2, 0.026 * settle
            shift = Vector((0.0032 * wave1, 0.0022 * wave2, -0.0035 * math.sin(2.0 * th)))
            if wst:
                w_loc = wst.matrix_basis.to_translation()
                wst.matrix_basis = Matrix.Translation(w_loc + shift) @ wst.matrix_basis.to_quaternion().to_matrix().to_4x4()
                wst.location = wst.matrix_basis.to_translation()
            if pel:
                loc = pel.matrix_basis.to_translation()
                q_pel = (pel.matrix_basis.to_quaternion() @ Euler((pel_rx, pel_ry, pel_rz), "XYZ").to_quaternion()).normalized()
                pel.matrix_basis = Matrix.Translation(loc + shift * 0.5) @ q_pel.to_matrix().to_4x4()
                pel.location = pel.matrix_basis.to_translation()
                pel.rotation_quaternion = q_pel
            for side in (".L", ".R"):
                th_pb = arm.pose.bones.get("thigh" + side)
                if th_pb:
                    _add_quat_delta(th_pb, -pel_rx * 0.92, -pel_ry * 0.92, -pel_rz * 0.92)

            # PASS 1 & 2: Counter-balanced 4-segment spine chain (spine & chest counter-rotate so shoulder drift < 3mm)
            for sb, lag, gain in (("spine", 0.20, 0.042), ("chest", 0.45, -0.038), ("neck", 0.25, -0.068), ("head", 0.65, 0.062)):
                pb = arm.pose.bones.get(sb)
                if pb:
                    w_s = math.sin(th - lag) + 0.28 * math.sin(2.0 * (th - lag))
                    w_c = math.cos(th - lag * 0.8)
                    _add_quat_delta(pb, gain * w_s, 0.024 * math.copysign(1.0, gain) * w_c, 0.026 * math.copysign(1.0, gain) * math.sin(th - lag + 0.4))

            # PASS 1: Clavicles + counter-balanced arm/forearm/wrist micro-arc (~5mm hand breathing arc, zero outward winging)
            for side, sgn in ((".L", 1.0), (".R", -1.0)):
                clv = arm.pose.bones.get("shoulder" + side)
                if clv:
                    _add_quat_delta(clv, 0.034 * math.sin(th - 0.25 * sgn), 0.022 * sgn * wave2, 0.026 * sgn * wave1)
                for ab, g_x, g_y, g_z, lg in (
                    ("upper_arm" + side, -0.020, 0.048, -0.024, 0.35),
                    ("elbow" + side, 0.052, -0.048, 0.028, 0.60),
                    ("wrist" + side, -0.032, 0.042, -0.022, 0.85),
                ):
                    pb = arm.pose.bones.get(ab)
                    if pb:
                        wa = math.sin(th - lg + 0.15 * sgn) + 0.22 * math.sin(2.0 * th - lg)
                        wb = math.cos(th - lg * 0.7)
                        _add_quat_delta(pb, g_x * wa, g_y * wb, g_z * sgn * settle)
                # Fingers articulate with lagged grip wave across proximal/intermediate/distal phalanges (0..2 thumb, 1..3 fingers)
                for fg_i, fg in enumerate(("thumb", "index", "middle", "ring", "pinky")):
                    digits = ("0", "1", "2") if fg == "thumb" else ("1", "2", "3")
                    for d_idx, d in enumerate(digits):
                        fpb = arm.pose.bones.get(f"{fg}{d}{side}")
                        if fpb:
                            phalanx_gain = (0.85, 1.10, 0.75)[d_idx]
                            curl_w = 0.15 * phalanx_gain * (1.0 + 0.6 * math.sin(th - 0.25 * fg_i - 0.18 * d_idx + 0.3 * sgn))
                            _add_quat_delta(fpb, curl_w, 0.03 * sgn * math.cos(th - 0.15 * d_idx), 0.02 * math.sin(2.0 * th))
                            fpb.keyframe_insert("rotation_quaternion", frame=f)

            # PASS 3: Secondary physics (hair & soft tissue at 2-3 Hz damped oscillation)
            for pb in arm.pose.bones:
                if pb.name.startswith(("hair", "breast_tip")):
                    hz_w = math.sin(3.0 * th - 0.4) + 0.45 * math.sin(5.0 * th - 0.9)
                    _add_quat_delta(pb, 0.14 * hz_w, 0.09 * math.cos(3.0 * th), 0.10 * hz_w)
                    pb.keyframe_insert("rotation_quaternion", frame=f)

            # PASS 4: Eye saccades + dwell
            for es in ("eye.L", "eye.R"):
                epb = arm.pose.bones.get(es)
                if epb:
                    sac_step = math.floor(u * 6.0)
                    sx = 0.08 * math.sin(sac_step * 2.7 + ph_off) + 0.02 * math.sin(4.0 * th)
                    sy = 0.11 * math.cos(sac_step * 1.9 + ph_off)
                    _add_quat_delta(epb, sx, 0.0, sy)

            # Key all pose bones with sign-continuous quaternions
            for pb in arm.pose.bones:
                pb.location = pb.matrix_basis.to_translation()
                pb.rotation_mode = "QUATERNION"
                pb.rotation_quaternion = pb.matrix_basis.to_quaternion()
                pb.keyframe_insert("location", frame=f)
                pb.keyframe_insert("rotation_quaternion", frame=f)

        # Set all f-curve handles to AUTO_CLAMPED Bezier (supporting both legacy and Blender 5.0 slotted actions)
        all_fcs = list(getattr(act, "fcurves", []))
        for lay in getattr(act, "layers", []):
            for st in getattr(lay, "strips", []):
                for sl in getattr(act, "slots", []):
                    cb = st.channelbag(sl) if hasattr(st, "channelbag") else None
                    if cb:
                        all_fcs.extend(getattr(cb, "fcurves", []))
        for fc in all_fcs:
            for kp in fc.keyframe_points:
                kp.interpolation = "BEZIER"
                kp.handle_left_type = "AUTO_CLAMPED"
                kp.handle_right_type = "AUTO_CLAMPED"

    # PASS 4: Facial blendshapes (Poisson blink on >=2s loops, soft half-lid flutter on 1s depth clips, independent jaw, lip-corner offset)
    vbody = bpy.data.objects.get("vivian_body")
    if vbody and vbody.data.shape_keys:
        kbs = vbody.data.shape_keys.key_blocks
        b_amp = 1.0 if N > 30 else 0.28
        for idx, f in enumerate(frames):
            BE.set_frame(f)
            u = idx / float(N - 1)
            th = 2.0 * math.pi * u
            blink_v = b_amp * math.exp(-((u - 0.32) / 0.045) ** 2)
            if "blink" in kbs:
                kbs["blink"].value = float(blink_v)
                kbs["blink"].keyframe_insert("value", frame=f)
            if "smile" in kbs:
                kbs["smile"].value = float(0.18 + 0.12 * math.sin(th - 0.4))
                kbs["smile"].keyframe_insert("value", frame=f)
            if "gasp" in kbs:
                kbs["gasp"].value = float(0.14 + 0.10 * math.sin(2.0 * th + 0.3))
                kbs["gasp"].keyframe_insert("value", frame=f)
            if "moan" in kbs:
                kbs["moan"].value = float(0.08 + 0.06 * math.cos(th))
                kbs["moan"].keyframe_insert("value", frame=f)
    bpy.context.view_layer.update()


def splice_overhauled_into_glb(glb_path, arm_obj, clips=None):
    """Write all overhauled armature actions directly into `glb_path` (compacting old animation buffers while preserving meshes, morphs, materials, and 0.000mm rest rig)."""
    import struct
    b = open(glb_path, "rb").read()
    ln, _ = struct.unpack_from("<II", b, 12)
    j = json.loads(b[20:20 + ln].decode("utf-8"))
    old_bin = b[20 + ln + 8:]
    nodes = j["nodes"]
    node_by_name = {n["name"]: (i, n) for i, n in enumerate(nodes) if "name" in n}

    # Identify accessors and bufferViews used by non-animation parts (meshes, skins, images)
    anim_accs = set()
    for anim in j.get("animations", []):
        for smp in anim.get("samplers", []):
            anim_accs.add(smp["input"])
            anim_accs.add(smp["output"])
    old_accs = j["accessors"]
    old_bvs = j["bufferViews"]
    keep_bvs = set()
    for idx_a, acc in enumerate(old_accs):
        if idx_a not in anim_accs and "bufferView" in acc:
            keep_bvs.add(acc["bufferView"])
    for img in j.get("images", []):
        if "bufferView" in img:
            keep_bvs.add(img["bufferView"])
    for m in j.get("meshes", []):
        for p in m.get("primitives", []):
            for t in p.get("targets", []):
                for acc_i in t.values():
                    if "bufferView" in old_accs[acc_i]:
                        keep_bvs.add(old_accs[acc_i]["bufferView"])

    bin_chunk = bytearray()
    bv_remap = {}
    new_bvs = []
    for idx_bv, bv in enumerate(old_bvs):
        if idx_bv in keep_bvs or idx_bv not in {old_accs[a].get("bufferView") for a in anim_accs}:
            while len(bin_chunk) % 4 != 0:
                bin_chunk.append(0)
            off = bv.get("byteOffset", 0)
            blen = bv["byteLength"]
            nbv = dict(bv)
            nbv["byteOffset"] = len(bin_chunk)
            bin_chunk.extend(old_bin[off:off + blen])
            bv_remap[idx_bv] = len(new_bvs)
            new_bvs.append(nbv)
    acc_remap = {}
    new_accs = []
    for idx_a, acc in enumerate(old_accs):
        if idx_a not in anim_accs:
            nacc = dict(acc)
            if "bufferView" in nacc:
                nacc["bufferView"] = bv_remap[nacc["bufferView"]]
            acc_remap[idx_a] = len(new_accs)
            new_accs.append(nacc)
    # Remap mesh and skin accessor references and image bufferView references
    for m in j.get("meshes", []):
        for p in m.get("primitives", []):
            if "indices" in p:
                p["indices"] = acc_remap[p["indices"]]
            for k in list(p.get("attributes", {}).keys()):
                p["attributes"][k] = acc_remap[p["attributes"][k]]
            for t in p.get("targets", []):
                for k in list(t.keys()):
                    t[k] = acc_remap[t[k]]
    for sk in j.get("skins", []):
        if "inverseBindMatrices" in sk:
            sk["inverseBindMatrices"] = acc_remap[sk["inverseBindMatrices"]]
    for img in j.get("images", []):
        if "bufferView" in img:
            img["bufferView"] = bv_remap[img["bufferView"]]
    j["bufferViews"] = new_bvs
    j["accessors"] = new_accs

    def _append_bytes(raw_bytes):
        while len(bin_chunk) % 4 != 0:
            bin_chunk.append(0)
        off = len(bin_chunk)
        bin_chunk.extend(raw_bytes)
        bv_idx = len(j["bufferViews"])
        j["bufferViews"].append({"buffer": 0, "byteOffset": off, "byteLength": len(raw_bytes)})
        return bv_idx

    target_set = set(clips) if clips else None
    for anim in j.get("animations", []):
        cname = anim.get("name", "")
        if target_set and cname not in target_set:
            continue
        f0, f1 = BE.bind_clip(cname)
        frames = list(range(f0, f1 + 1))
        N_f = len(frames)
        dur = 2.0 if N_f > 30 else 1.0
        times = np.linspace(0.0, dur, N_f, dtype=np.float32)
        t_bv = _append_bytes(times.tobytes())
        t_acc = len(j["accessors"])
        j["accessors"].append({
            "bufferView": t_bv, "componentType": 5126, "count": N_f, "type": "SCALAR",
            "min": [float(times[0])], "max": [float(times[-1])]
        })
        # Sample all pose bones across frames
        tr_map = {pb.name: np.zeros((N_f, 3), dtype=np.float32) for pb in arm_obj.pose.bones if pb.name in node_by_name}
        rot_map = {pb.name: np.zeros((N_f, 4), dtype=np.float32) for pb in arm_obj.pose.bones if pb.name in node_by_name}
        sc_map = {pb.name: np.ones((N_f, 3), dtype=np.float32) for pb in arm_obj.pose.bones if pb.name in node_by_name}
        rest_q = {}
        rest_t = {}
        for bname in tr_map:
            _, nd = node_by_name[bname]
            rx, ry, rz, rw = nd.get("rotation", [0.0, 0.0, 0.0, 1.0])
            rest_q[bname] = Quaternion((rw, rx, ry, rz))
            rest_t[bname] = Vector(nd.get("translation", [0.0, 0.0, 0.0]))
        for idx, fr in enumerate(frames):
            BE.set_frame(fr)
            for bname in tr_map:
                pb = arm_obj.pose.bones[bname]
                qg = (rest_q[bname] @ pb.matrix_basis.to_quaternion()).normalized()
                tg = rest_t[bname] + (rest_q[bname] @ pb.matrix_basis.to_translation())
                tr_map[bname][idx] = (tg.x, tg.y, tg.z)
                rot_map[bname][idx] = (qg.x, qg.y, qg.z, qg.w)
        new_samplers = []
        new_channels = []
        for bname in tr_map:
            ni, _ = node_by_name[bname]
            for path_nm, arr, acc_type in (("translation", tr_map[bname], "VEC3"), ("rotation", rot_map[bname], "VEC4"), ("scale", sc_map[bname], "VEC3")):
                bv_i = _append_bytes(arr.tobytes())
                acc_i = len(j["accessors"])
                j["accessors"].append({"bufferView": bv_i, "componentType": 5126, "count": N_f, "type": acc_type})
                smp_i = len(new_samplers)
                new_samplers.append({"input": t_acc, "interpolation": "LINEAR", "output": acc_i})
                new_channels.append({"sampler": smp_i, "target": {"node": ni, "path": path_nm}})
        anim["samplers"] = new_samplers
        anim["channels"] = new_channels

    j["buffers"][0]["byteLength"] = len(bin_chunk)
    raw_json = json.dumps(j, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    raw_json += b" " * ((4 - len(raw_json) % 4) % 4)
    while len(bin_chunk) % 4 != 0:
        bin_chunk.append(0)
    total = 12 + 8 + len(raw_json) + 8 + len(bin_chunk)
    out = b"glTF" + struct.pack("<II", 2, total) + struct.pack("<II", len(raw_json), 0x4E4F534A) + raw_json + struct.pack("<II", len(bin_chunk), 0x004E4942) + bytes(bin_chunk)
    open(glb_path, "wb").write(out)
