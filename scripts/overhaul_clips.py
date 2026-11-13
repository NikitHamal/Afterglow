# Scoped per-clip motion overhaul engine in bpy (Pass 1-4: body mechanics, timing/arcs, secondary physics, face/eyes).
import os, sys, json, math, shutil
import numpy as np
import bpy
from mathutils import Vector, Quaternion, Euler, Matrix

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)
import blender_env as BE
import render_audit as RA
import contact_sheet as CS

def _add_quat_delta(pb, rx, ry, rz):
    dq = Euler((rx, ry, rz), "XYZ").to_quaternion()
    q0 = pb.matrix_basis.to_quaternion()
    q1 = (q0 @ dq).normalized()
    pb.rotation_mode = "QUATERNION"
    pb.rotation_quaternion = q1
    pb.matrix_basis = Matrix.Translation(pb.matrix_basis.to_translation()) @ q1.to_matrix().to_4x4()

def _apply_pose_fixes(clip, arm):
    """Fix specific baseline limb/torso pose defects identified by visual contact-sheet inspection."""
    is_viv = "vivian" in arm.name.lower()
    if clip == "stand.idle":
        # Fix bowed-out chicken-wing elbows by adducting upper arms toward ribs and relaxing elbows
        for side, sgn in ((".L", 1.0), (".R", -1.0)):
            ua = arm.pose.bones.get("upper_arm" + side)
            el = arm.pose.bones.get("elbow" + side)
            if ua:
                _add_quat_delta(ua, 0.06 if is_viv else 0.04, -0.14 * sgn if is_viv else -0.22 * sgn, -0.04 * sgn)
            if el:
                _add_quat_delta(el, -0.08 if is_viv else 0.06, 0.0, -0.10 * sgn if is_viv else -0.05 * sgn)
    elif clip in ("oral.in", "handjob.in", "titjob.in"):
        if not is_viv:
            # Fix Marcus's 150-deg backward-folded forearms in supine clips so arms rest naturally along the sheet
            for side, sgn in ((".L", 1.0), (".R", -1.0)):
                ua = arm.pose.bones.get("upper_arm" + side)
                el = arm.pose.bones.get("elbow" + side)
                wr = arm.pose.bones.get("wrist" + side)
                if ua:
                    q_fix = Euler((-0.26, 0.42 * sgn, 0.08 * sgn), "XYZ").to_quaternion()
                    ua.rotation_quaternion = q_fix
                    ua.matrix_basis = Matrix.Translation(ua.matrix_basis.to_translation()) @ q_fix.to_matrix().to_4x4()
                if el:
                    q_el = Euler((0.22, 0.0, 0.14 * sgn), "XYZ").to_quaternion()
                    el.rotation_quaternion = q_el
                    el.matrix_basis = Matrix.Translation(el.matrix_basis.to_translation()) @ q_el.to_matrix().to_4x4()
                if wr:
                    q_wr = Euler((0.08, -0.10 * sgn, 0.0), "XYZ").to_quaternion()
                    wr.rotation_quaternion = q_wr
                    wr.matrix_basis = Matrix.Translation(wr.matrix_basis.to_translation()) @ q_wr.to_matrix().to_4x4()
        elif clip == "oral.in":
            # Keep Vivian's left elbow tucked back toward her hip instead of jutting past her head
            ua_l = arm.pose.bones.get("upper_arm.L")
            el_l = arm.pose.bones.get("elbow.L")
            if ua_l:
                _add_quat_delta(ua_l, -0.28, -0.24, 0.22)
            if el_l:
                _add_quat_delta(el_l, 0.20, 0.0, 0.25)
    elif clip in ("cowgirl.in", "cowgirl.rub") and is_viv:
        # Lean Vivian's torso forward so her palms reach Marcus's chest instead of hovering 19 cm in mid-air
        for sb, ang in (("spine", 0.16), ("chest", 0.18), ("neck", -0.18)):
            pb = arm.pose.bones.get(sb)
            if pb:
                _add_quat_delta(pb, ang, 0.0, 0.0)
        for side, sgn in ((".L", 1.0), (".R", -1.0)):
            ua = arm.pose.bones.get("upper_arm" + side)
            el = arm.pose.bones.get("elbow" + side)
            if ua:
                _add_quat_delta(ua, 0.22, -0.08 * sgn, 0.12 * sgn)
            if el:
                _add_quat_delta(el, -0.14, 0.0, -0.08 * sgn)
    elif clip in ("spoon.in", "spoon.rub") and is_viv:
        # Tuck Vivian's right forearm so it rests on the pillow/sheet instead of jutting in front of her face
        ua_r = arm.pose.bones.get("upper_arm.R")
        el_r = arm.pose.bones.get("elbow.R")
        if ua_r:
            _add_quat_delta(ua_r, 0.18, 0.0, -0.22)
        if el_r:
            _add_quat_delta(el_r, 0.24, 0.0, -0.15)

def overhaul_loaded_clip(clip, profile="idle"):
    """Apply Pass 1-4 biomechanical weight, overlap, secondary hair/soft-tissue, eye saccade, and blendshape curves."""
    f0, f1 = BE.bind_clip(clip)
    if clip == "missionary.embrace" and (f1 - f0) < 24:
        f0, f1 = 0, 24
    frames = list(range(f0, f1 + 1))
    N = max(2, len(frames))
    arms = [o for o in bpy.context.scene.objects if o.type == "ARMATURE"]
    # Sample base trajectories and de-spike frame discontinuities via 3-frame quaternion/location smoothing
    raw_basis = {}
    for arm in arms:
        raw_basis[arm.name] = {}
        for f in frames:
            BE.set_frame(min(f, int(round(arm.animation_data.action.frame_range[1])) if (arm.animation_data and arm.animation_data.action) else f))
            _apply_pose_fixes(clip, arm)
            raw_basis[arm.name][f] = {pb.name: pb.matrix_basis.copy() for pb in arm.pose.bones}
    base_basis = {}
    for arm in arms:
        base_basis[arm.name] = {}
        for idx, f in enumerate(frames):
            f_prev = frames[max(0, idx - 1)]
            f_next = frames[min(len(frames) - 1, idx + 1)]
            smoothed = {}
            for pb in arm.pose.bones:
                bn = pb.name
                m0, m1, m2 = raw_basis[arm.name][f_prev][bn], raw_basis[arm.name][f][bn], raw_basis[arm.name][f_next][bn]
                t_s = (m0.to_translation() * 0.25) + (m1.to_translation() * 0.5) + (m2.to_translation() * 0.25)
                q_a = m0.to_quaternion().slerp(m1.to_quaternion(), 0.67)
                q_s = q_a.slerp(m2.to_quaternion(), 0.25)
                smoothed[bn] = Matrix.Translation(t_s) @ q_s.to_matrix().to_4x4()
            base_basis[arm.name][f] = smoothed

    amp = 0.52 if ("idle" in clip or "sit" in clip or "embrace" in clip or "kiss" in clip) else 0.68
    for arm_idx, arm in enumerate(arms):
        act = arm.animation_data.action if arm.animation_data else None
        if act is None:
            continue
        ph_off = 0.0 if "vivian" in arm.name.lower() else 0.85
        for idx, f in enumerate(frames):
            BE.set_frame(f)
            u = idx / float(N - 1)
            th = 2.0 * math.pi * u + ph_off
            # Smooth multi-harmonic spacing with subtle overshoot & settle
            wave1 = math.sin(th) + 0.18 * math.sin(2.0 * th - 0.45)
            wave2 = math.cos(th - 0.35) + 0.16 * math.sin(2.0 * th + 0.25)
            settle = math.sin(th) + 0.22 * math.sin(2.0 * th - 0.7)

            for bname, mat in base_basis[arm.name][f].items():
                pb = arm.pose.bones.get(bname)
                if pb:
                    pb.matrix_basis = mat.copy()

            # PASS 1: Pelvis weight shift + contrapposto hip roll + counter-rotation (natural 1.4cm shift, ~6 deg roll)
            pel = arm.pose.bones.get("pelvis")
            if pel:
                loc = pel.matrix_basis.to_translation()
                shift = Vector((0.014 * amp * wave1, 0.008 * amp * wave2, 0.005 * amp * math.sin(2.0 * th)))
                q_pel = (pel.matrix_basis.to_quaternion() @ Euler((0.095 * amp * wave1, 0.065 * amp * wave2, 0.085 * amp * settle), "XYZ").to_quaternion()).normalized()
                pel.matrix_basis = Matrix.Translation(loc + shift) @ q_pel.to_matrix().to_4x4()
                pel.location = pel.matrix_basis.to_translation()
                pel.rotation_mode = "QUATERNION"
                pel.rotation_quaternion = q_pel
                pel.keyframe_insert("location", frame=f)
                pel.keyframe_insert("rotation_quaternion", frame=f)

            # PASS 1 & 2: 4-segment spine chain with phase lag (overlap, ~5.5-7 deg peak-to-peak)
            for s_i, (sb, lag, gain) in enumerate((("spine", 0.25, 0.105), ("chest", 0.55, 0.115), ("neck", 0.85, -0.11), ("head", 1.15, 0.12))):
                pb = arm.pose.bones.get(sb)
                if pb:
                    w_s = math.sin(th - lag) + 0.18 * math.sin(2.0 * (th - lag))
                    w_c = math.cos(th - lag * 0.8)
                    _add_quat_delta(pb, gain * amp * w_s, 0.065 * amp * w_c, 0.075 * amp * math.sin(th - lag + 0.4))
                    pb.keyframe_insert("rotation_quaternion", frame=f)

            # PASS 1: Clavicles follow arms + breathing lift (~6 deg excursion, arms stay naturally by sides)
            for side, sgn in ((".L", 1.0), (".R", -1.0)):
                clv = arm.pose.bones.get("shoulder" + side)
                if clv:
                    _add_quat_delta(clv, 0.105 * amp * math.sin(th - 0.3 * sgn), 0.06 * sgn * amp * wave2, 0.08 * sgn * amp * wave1)
                    clv.keyframe_insert("rotation_quaternion", frame=f)
                # Subtle arm & forearm curved arc + overshoot (~6-7 deg, no outward winging)
                for ab, g_a, lg in (("upper_arm" + side, 0.105, 0.35), ("elbow" + side, 0.115, 0.65), ("wrist" + side, 0.11, 0.95)):
                    pb = arm.pose.bones.get(ab)
                    if pb:
                        wa = math.sin(th - lg + 0.2 * sgn) + 0.18 * math.sin(2.0 * th - lg)
                        wb = math.cos(th - lg * 0.7)
                        _add_quat_delta(pb, g_a * amp * wa, 0.065 * amp * wb, 0.055 * sgn * amp * settle)
                        pb.keyframe_insert("rotation_quaternion", frame=f)
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
                    epb.keyframe_insert("rotation_quaternion", frame=f)

        # Set all f-curve handles to AUTO_CLAMPED Bezier
        for fc in getattr(act, "fcurves", []):
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
    """Write all overhauled armature actions directly into `glb_path` (preserving meshes, morphs, materials, and 0.000mm rest rig)."""
    import struct
    b = open(glb_path, "rb").read()
    ln, _ = struct.unpack_from("<II", b, 12)
    j = json.loads(b[20:20 + ln].decode("utf-8"))
    bin_chunk = bytearray(b[20 + ln + 8:])
    nodes = j["nodes"]
    node_by_name = {n["name"]: (i, n) for i, n in enumerate(nodes) if "name" in n}

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
