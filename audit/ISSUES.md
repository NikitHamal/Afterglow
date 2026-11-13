# Afterglow Animation Issue Board (`audit/ISSUES.md`)

## Issue Table (Sorted by Scene Priority × Beat Centrality × Severity)

| ID | Clip | Scene | Beat | Failing Field | 12-Principles Tag | Pass | Status | Commit | Summary |
|---|---|---|---|---|---|---|---|---|---|
| `AF-000` | `all` | `all` | `bootstrap` | `status_tracking` | `Staging` | `Pass 0` | `verified` | `0774aa9` | Missing `ANIMATION_STATUS.md`, `audit/ISSUES.md`, and `audit/INVENTORY.md` initialized. |
| `AF-001` | `stand.idle` | `the_massage` | `c1_lobby` | `pose_diversity`, `secondary_motion_hz`, `clavicle_active`, `finger_articulation`, `overshoot_present` | `Overlap` / `Timing` | `Pass 1-4` | `verified` | `HEAD` | Multi-harmonic contrapposto pelvis shift, 4-segment spine lag, clavicle follow, finger curl wave, 3Hz hair/soft-tissue, saccades & blendshape layering (`doll_score` 20.0 → 100.0). |
| `AF-002` | `stand.embrace` | `the_massage` | `c2_steam` | `pose_diversity`, `clavicle_active`, `neck_counter_rotated`, `overshoot_present` | `Anticipation` / `Overlap` | `Pass 1` | `open` | — | Static cheek-on-chest solve with single-sine breath and rigid arms/hands on partner back. |
| `AF-003` | `kiss.stand` | `the_massage` | `c3_lesson` | `pose_diversity`, `clavicle_active`, `finger_articulation`, `secondary_motion_hz` | `Slow-in-out` / `Secondary Action` | `Pass 1` | `open` | — | Kiss nod is a uniform 1.2 deg sine wave; hands/fingers stay frozen on nape/waist. |
| `AF-004` | `sit.bed` | `the_massage` | `c4_turn` | `pose_diversity`, `clavicle_active`, `finger_articulation`, `root_shift_sd` | `Weight` / `Arcs` | `Pass 1` | `open` | — | Seated idle has dead pelvis (`root_shift_sd ≈ 0`), static thighs/hands, and uniform sine breath. |
| `AF-005` | `kiss.sit` | `the_massage` | `c4_turn` | `pose_diversity`, `clavicle_active`, `finger_articulation`, `overshoot_present` | `Timing` / `Staging` | `Pass 1` | `open` | — | Seated kiss locks pelvis and far hands on thighs with zero weight transfer or hand caress. |

## Qualitative Visual Tells (Human Inspection Section)
1. **Single-Frequency Sinusoidal Motion (`sin(2*pi*t)`)**: All story loops (`stand.idle`, `stand.embrace`, `kiss.stand`, `sit.bed`, `kiss.sit`) drive breathing, weight shift, and head nod from the exact same `ph = 2 * math.pi * t` harmonic without phase desynchronization across spine/clavicle/head/fingers, reading as mechanical clockwork.
2. **Dead Clavicles & Rigid Spine Block**: `body.spine()` only sets 2 orientations (`上半身` and `上半身2`), while clavicles (`肩.L`, `肩.R`) are never driven by arm elevation or breathing, leaving shoulders pinned in T-pose rest orientation.
3. **Frozen Finger Curls**: `body.fingers(s, 0.38)` sets an identical static curl across all fingers and frames; hands never adjust grip pressure, spread, or micro-caress during embraces or kisses.
4. **Skipped Hair & Eye Keys in GLBClips**: `Rig.SKIP_KEY = ("髪", "目", "竿端")` omits hair and eye bones from baked clips, resulting in zero secondary hair oscillation (`secondary_motion_hz = 0`) and zero saccades/eye tracking in standalone clip playback.
5. **Facial Expression & Jaw Separation**: Story clips only key `vag_open` / `aroused` in `author.py` while facial blendshapes (`blink`, `brow`, `jaw`, `smile`, visemes) are absent from baked actions or snap without 2–4 frame crossfades and independent jaw/lip-corner layering.
