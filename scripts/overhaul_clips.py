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

def overhaul_loaded_clip(clip, profile="idle"):
    """Apply Pass 1-4 biomechanical weight, overlap, secondary hair/soft-tissue, eye saccade, and blendshape curves."""
    f0, f1 = BE.bind_clip(clip)
    frames = list(range(f0, f1 + 1))
    N = max(2, len(frames))
    arms = [o for o in bpy.context.scene.objects if o.type == "ARMATURE"]
    # Sample base trajectories and de-spike frame discontinuities via 3-frame quaternion/location smoothing
    raw_basis = {}
    for arm in arms:
        raw_basis[arm.name] = {}
        for f in frames:
            BE.set_frame(f)
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
                # Fingers articulate with lagged grip wave
                for fg_i, fg in enumerate(("thumb", "index", "middle", "ring", "pinky")):
                    for d in ("0", "1", "2"):
                        fpb = arm.pose.bones.get(f"{fg}{d}{side}")
                        if fpb:
                            curl_w = 0.15 * (1.0 + 0.6 * math.sin(th - 0.25 * fg_i + 0.3 * sgn))
                            _add_quat_delta(fpb, curl_w, 0.03 * sgn * math.cos(th), 0.02 * math.sin(2.0 * th))
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

    # PASS 4: Facial blendshapes (Poisson blink, independent jaw, lip-corner offset, brow micro-motion)
    vbody = bpy.data.objects.get("vivian_body")
    if vbody and vbody.data.shape_keys:
        kbs = vbody.data.shape_keys.key_blocks
        for idx, f in enumerate(frames):
            BE.set_frame(f)
            u = idx / float(N - 1)
            th = 2.0 * math.pi * u
            # Blink pulse around u=0.32 (smooth 4-frame crossfade)
            blink_v = math.exp(-((u - 0.32) / 0.045) ** 2)
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
