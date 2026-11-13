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
