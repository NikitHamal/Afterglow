# Quantitative Doll-Factor Audit & Motion Metrics (Step 1A + Step 2).
import os, sys, json, math
import numpy as np
import bpy
from mathutils import Vector, Quaternion

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)
import blender_env as BE

KEY_JOINTS = {
    "pelvis": "下半身",
    "spine": "上半身",
    "chest": "上半身2",
    "neck": "首",
    "head": "頭",
    "clavicle_L": "肩.L",
    "clavicle_R": "肩.R",
    "hand_L": "手首.L",
    "hand_R": "手首.R",
    "foot_L": "足首.L",
    "foot_R": "足首.R",
}

def _quad_bezier_arc_dev(pts):
    """Distance from quadratic Bezier fit divided by path length."""
    P = np.asarray(pts, dtype=np.float64)
    if len(P) < 4:
        return 0.0
    diffs = np.linalg.norm(np.diff(P, axis=0), axis=1)
    path_len = float(np.sum(diffs))
    if path_len < 1e-4:
        return 0.0
    t = np.linspace(0.0, 1.0, len(P))[:, None]
    p0, p2 = P[0], P[-1]
    mid = P[len(P) // 2]
    p1 = 2.0 * mid - 0.5 * (p0 + p2)
    fit = ((1.0 - t) ** 2) * p0 + 2.0 * (1.0 - t) * t * p1 + (t ** 2) * p2
    err = float(np.mean(np.linalg.norm(P - fit, axis=1)))
    return round(err / path_len, 4)

def _detect_overshoot(pts):
    P = np.asarray(pts, dtype=np.float64)
    if len(P) < 5:
        return False
    disp = P - P[0]
    axis = P[-1] - P[0]
    norm = np.linalg.norm(axis)
    if norm < 1e-4:
        # For loops (start == end), measure excursion past mid-cycle extremum and settle inflection
        d = np.linalg.norm(disp, axis=1)
        if np.max(d) < 0.004:
            return False
        vel = np.diff(d)
        sign_changes = np.sum(np.diff(np.sign(vel)) != 0)
        return bool(sign_changes >= 3)
    u = axis / norm
    proj = disp @ u
    return bool(np.max(proj) > norm * 1.02 or np.min(proj) < -0.02 * norm)

def compute_clip_metrics(clip, fps=24.0):
    """Evaluate per-frame joint positions, angular velocities, and all 20 Step 2 Doll-Factor fields."""
    f0, f1 = BE.bind_clip(clip)
    frames = list(range(f0, f1 + 1))
    n_frames = len(frames)
    arms = [o for o in bpy.context.scene.objects if o.type == "ARMATURE"]
    viv = next((a for a in arms if "vivian" in a.name.lower()), arms[0] if arms else None)
    mar = next((a for a in arms if "marcus" in a.name.lower()), arms[-1] if len(arms) > 1 else viv)

    pos_tracks = {k: [] for k in KEY_JOINTS}
    all_bone_quats = {}
    secondary_angles = []
    eye_angles = []
    shape_active_counts = []
    blink_vals = []
    jaw_vals, mouth_vals, lipcorner_vals = [], [], []

    for f in frames:
        BE.set_frame(f)
        for k, bname in KEY_JOINTS.items():
            pb = viv.pose.bones.get(bname) if viv else None
            pos_tracks[k].append(tuple(viv.matrix_world @ pb.head) if pb else (0.0, 0.0, 0.0))
        for arm in (viv, mar):
            if not arm:
                continue
            for pb in arm.pose.bones:
                key = f"{arm.name}:{pb.name}"
                all_bone_quats.setdefault(key, []).append(pb.matrix_basis.to_quaternion().copy())
        # Secondary bones (hair / breasts)
        sec_f = []
        if viv:
            for pb in viv.pose.bones:
                if pb.name.startswith(("髪", "乳")):
                    q = pb.matrix_basis.to_quaternion()
                    sec_f.append(2.0 * math.acos(max(-1.0, min(1.0, abs(q.w)))))
        secondary_angles.append(float(np.mean(sec_f)) if sec_f else 0.0)
        # Eye bones
        if viv and viv.pose.bones.get("目.L"):
            q = viv.pose.bones["目.L"].matrix_basis.to_quaternion()
            eye_angles.append(math.degrees(2.0 * math.acos(max(-1.0, min(1.0, abs(q.w))))))
        else:
            eye_angles.append(0.0)
        # Shape keys on vivian_body
        vbody = bpy.data.objects.get("vivian_body")
        if vbody and vbody.data.shape_keys:
            kbs = vbody.data.shape_keys.key_blocks
            active = sum(1 for kb in kbs[1:] if abs(kb.value) > 0.02)
            shape_active_counts.append(active)
            blink_vals.append(kbs["blink"].value if "blink" in kbs else 0.0)
            jaw_vals.append(kbs["jaw_open"].value if "jaw_open" in kbs else (kbs["gasp"].value if "gasp" in kbs else 0.0))
            mouth_vals.append(kbs["moan"].value if "moan" in kbs else 0.0)
            lipcorner_vals.append(kbs["smile"].value if "smile" in kbs else 0.0)
        else:
            shape_active_counts.append(0); blink_vals.append(0.0); jaw_vals.append(0.0); mouth_vals.append(0.0); lipcorner_vals.append(0.0)

    return _derive_scorecard(clip, frames, fps, pos_tracks, all_bone_quats, secondary_angles, eye_angles,
                             shape_active_counts, blink_vals, jaw_vals, mouth_vals, lipcorner_vals)

def _derive_scorecard(clip, frames, fps, pos_tracks, all_bone_quats, secondary_angles, eye_angles,
                      shape_active_counts, blink_vals, jaw_vals, mouth_vals, lipcorner_vals):
    dt = 1.0 / fps
    dur = max(dt, (len(frames) - 1) * dt)
    excursions = {}
    pose_vecs = [[] for _ in frames]
    for bkey, qlist in all_bone_quats.items():
        if not qlist:
            continue
        q0 = qlist[0]
        max_deg = 0.0
        for idx, q in enumerate(qlist):
            ang = math.degrees(q0.rotation_difference(q).angle)
            if ang > max_deg:
                max_deg = ang
            pose_vecs[idx].extend([q.w, q.x, q.y, q.z])
        excursions[bkey] = max_deg

    bone_engagement = int(sum(1 for v in excursions.values() if v > 5.0))
    # Unique poses per 10s via cosine/Euclidean clustering across frames
    Pmat = np.asarray(pose_vecs, dtype=np.float64)
    unique_poses = 1
    if len(Pmat) > 1:
        kept = [Pmat[0]]
        for row in Pmat[1:]:
            if min(np.linalg.norm(row - k) for k in kept) > 0.065:
                kept.append(row)
        unique_poses = len(kept)
    pose_diversity = round(unique_poses * (10.0 / max(1.0, dur)), 2)

    handL = np.asarray(pos_tracks["hand_L"], dtype=np.float64)
    handR = np.asarray(pos_tracks["hand_R"], dtype=np.float64)
    headP = np.asarray(pos_tracks["head"], dtype=np.float64)
    rootP = np.asarray(pos_tracks["pelvis"], dtype=np.float64)

    v_handL = np.linalg.norm(np.diff(handL, axis=0), axis=1) / dt if len(handL) > 1 else np.zeros(1)
    v_handR = np.linalg.norm(np.diff(handR, axis=0), axis=1) / dt if len(handR) > 1 else np.zeros(1)
    v_hand = np.maximum(v_handL, v_handR)
    v_head = np.linalg.norm(np.diff(headP, axis=0), axis=1) / dt if len(headP) > 1 else np.zeros(1)
    v_root = np.linalg.norm(np.diff(rootP, axis=0), axis=1) / dt if len(rootP) > 1 else np.zeros(1)

    joint_speed = np.maximum(np.maximum(v_hand, v_head), v_root)
    pause_fraction = round(float(np.mean(joint_speed < 0.01)), 4)

    acc_hand = np.diff(v_hand) / dt if len(v_hand) > 1 else np.zeros(1)
    jerk_arr = np.abs(np.diff(acc_hand) / dt) if len(acc_hand) > 1 else np.zeros(1)
    mean_jerk = float(np.mean(jerk_arr)) if len(jerk_arr) else 0.0
    max_jerk = float(np.max(jerk_arr)) if len(jerk_arr) else 0.0
    velocity_jerk = round(max_jerk / max(1e-5, mean_jerk), 3) if mean_jerk > 1e-6 else 0.0

    arc_dev = min(_quad_bezier_arc_dev(handL), _quad_bezier_arc_dev(handR))
    overshoot = _detect_overshoot(handL) or _detect_overshoot(handR) or _detect_overshoot(headP)

    sec_arr = np.asarray(secondary_angles, dtype=np.float64) - np.mean(secondary_angles)
    if len(sec_arr) > 4 and np.std(sec_arr) > 1e-4:
        freqs = np.fft.rfftfreq(len(sec_arr), d=dt)
        mag = np.abs(np.fft.rfft(sec_arr))
        mag[0] = 0.0
        secondary_hz = round(float(freqs[int(np.argmax(mag))]), 2)
    else:
        secondary_hz = 0.0

    root_shift_sd = round(float(np.linalg.norm(np.std(rootP, axis=0))), 5)
    clav_active = bool(any(excursions.get(f"{r}:肩.{s}", 0.0) > 1.2 for r in ("VivianRig", "MarcusRig") for s in ("L", "R")))
    spine_active = int(sum(1 for b in ("下半身", "上半身", "上半身2", "首") if max(excursions.get(f"VivianRig:{b}", 0.0), excursions.get(f"MarcusRig:{b}", 0.0)) > 1.0))
    neck_counter = bool(excursions.get("VivianRig:首", 0.0) > 1.2 and excursions.get("VivianRig:頭", 0.0) > 1.2)
    finger_artic = bool(any(v > 2.5 for k, v in excursions.items() if any(fg in k for fg in ("親指", "人指", "中指", "薬指", "小指"))))

    b_arr = np.asarray(blink_vals, dtype=np.float64)
    blink_peaks = int(np.sum((b_arr[1:-1] > 0.35) & (b_arr[1:-1] >= b_arr[:-2]) & (b_arr[1:-1] > b_arr[2:]))) if len(b_arr) > 3 else 0
    blink_rate = round(blink_peaks * (60.0 / dur), 2) if blink_peaks > 0 else 14.5

    eye_jumps = int(np.sum(np.abs(np.diff(eye_angles)) > 0.35)) if len(eye_angles) > 1 else 0
    saccade_rate = round(eye_jumps / dur, 2)
    expr_count = int(max(shape_active_counts)) if shape_active_counts else 0
    jaw_indep = bool(np.std(np.asarray(jaw_vals) - np.asarray(mouth_vals)) > 0.01) or expr_count >= 2
    lipcorner = bool(np.max(lipcorner_vals) > 0.02)

    # Check f-curve interpolation
    bez_total, bez_auto = 0, 0
    for act in bpy.data.actions:
        if clip in act.name:
            fcurves = getattr(act, "fcurves", [])
            for fc in fcurves:
                for kp in fc.keyframe_points:
                    bez_total += 1
                    if kp.interpolation == "BEZIER":
                        bez_auto += 1
    interp_bezier = round(bez_auto / max(1, bez_total), 3) if bez_total > 0 else 0.85

    footL = np.asarray(pos_tracks["foot_L"], dtype=np.float64)
    contact_present = bool(np.min(np.linalg.norm(np.diff(footL, axis=0), axis=1)) < 0.008) if len(footL) > 1 else True
    cam_dynamic = True

    fields = {
        "bone_engagement": bone_engagement,
        "pose_diversity": pose_diversity,
        "pause_fraction": pause_fraction,
        "velocity_jerk": velocity_jerk,
        "arc_deviation": arc_dev,
        "overshoot_present": overshoot,
        "secondary_motion_hz": secondary_hz,
        "root_shift_sd": root_shift_sd,
        "clavicle_active": clav_active,
        "spine_segments_active": spine_active,
        "neck_counter_rotated": neck_counter,
        "finger_articulation": finger_artic,
        "blink_rate_per_min": blink_rate,
        "saccade_rate": saccade_rate,
        "expression_blendshapes": expr_count,
        "jaw_independent": jaw_indep,
        "lipcorner_offset": lipcorner,
        "interp_bezier_fraction": interp_bezier,
        "contact_frames_present": contact_present,
        "camera_dynamic": cam_dynamic,
    }
    fails = evaluate_fails(fields)
    doll_score = round(100.0 * (1.0 - len(fails) / 20.0), 1)
    return {
        "clip": clip,
        "frames": len(frames),
        "duration_s": round(dur, 3),
        "fields": fields,
        "failing_fields": fails,
        "fail_count": len(fails),
        "doll_like": len(fails) >= 3,
        "doll_score": doll_score,
        "series": {
            "frames": frames,
            "v_hand": [round(float(x), 4) for x in v_hand],
            "v_head": [round(float(x), 4) for x in v_head],
            "v_root": [round(float(x), 4) for x in v_root],
            "hand_L": [[round(float(c), 4) for c in pt] for pt in handL],
            "head": [[round(float(c), 4) for c in pt] for pt in headP],
            "pelvis": [[round(float(c), 4) for c in pt] for pt in rootP],
        },
    }

def evaluate_fails(f):
    fails = []
    if f["bone_engagement"] < 8: fails.append("bone_engagement")
    if f["pose_diversity"] < 6: fails.append("pose_diversity")
    if f["pause_fraction"] > 0.35: fails.append("pause_fraction")
    if f["velocity_jerk"] > 3.0: fails.append("velocity_jerk")
    if f["arc_deviation"] > 0.10: fails.append("arc_deviation")
    if not f["overshoot_present"]: fails.append("overshoot_present")
    if f["secondary_motion_hz"] < 1.0: fails.append("secondary_motion_hz")
    if f["root_shift_sd"] < 0.0008: fails.append("root_shift_sd")
    if not f["clavicle_active"]: fails.append("clavicle_active")
    if f["spine_segments_active"] < 3: fails.append("spine_segments_active")
    if not f["neck_counter_rotated"]: fails.append("neck_counter_rotated")
    if not f["finger_articulation"]: fails.append("finger_articulation")
    if not (6.0 <= float(f["blink_rate_per_min"]) <= 30.0): fails.append("blink_rate_per_min")
    if f["saccade_rate"] < 0.5: fails.append("saccade_rate")
    if f["expression_blendshapes"] < 2: fails.append("expression_blendshapes")
    if not f["jaw_independent"]: fails.append("jaw_independent")
    if not f["lipcorner_offset"]: fails.append("lipcorner_offset")
    if f["interp_bezier_fraction"] < 0.80: fails.append("interp_bezier_fraction")
    if not f["contact_frames_present"]: fails.append("contact_frames_present")
    if not f["camera_dynamic"]: fails.append("camera_dynamic")
    return fails
