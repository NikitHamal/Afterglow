# Afterglow Animation Overhaul — Changelog (`audit/CHANGELOG.md`)

## Bootstrap & Review Apparatus (`Pass 0`)
- `AF-000` (`0774aa9`) | `all` | Initialized `ANIMATION_STATUS.md`, `audit/ISSUES.md`, `audit/INVENTORY.md`, `scripts/{blender_env,motion_metrics,render_audit,contact_sheet}.py`, `notebooks/dashboard.py`, and full 21-clip baseline audit (`mean doll_score = 32.4`).

## Scene 1: `the_massage` ("The Massage")
- `AF-001` (`2a87f64`) | `stand.idle` (`c1_lobby`) | `doll_score`: **20.0 → 100.0** (+80.0), fails: **16 → 0** (`bone_engagement` 0→108, `pose_diversity` 5.0→240.0, `pause_fraction` 0.9167→0.0, `velocity_jerk` 7.604→1.34, `arc_deviation` 0.1535→0.0412, `secondary_motion_hz` 0.0→2.94, `root_shift_sd` 0.0→0.00626).
- `AF-001b` (`2a87f64`, `closed-negative`) | `stand.idle` (`c1_lobby`) | Reverted `amp=1.25` (`velocity_jerk=4.421`, outward arm winging in `compare_3q.png`) to `amp=0.52` + 3-frame base trajectory de-spiking and LayerWeight Matcap clay shader.
- `AF-002` (`fd51c52`) | `sit.bed` (`c4_turn`) | `doll_score`: **35.0 → 95.0** (+60.0), fails: **13 → 1** (`bone_engagement` 2→106, `clavicle_active` False→True, `finger_articulation` False→True, `secondary_motion_hz` 0.0→2.94).

## Scene 2: `the_storm` ("The Storm")
- `AF-003` (`d9c1f4d`) | `missionary.in` (`c2_cabin`) | `doll_score`: **35.0 → 95.0** (+60.0), fails: **13 → 1** (`bone_engagement` 4→110, `spine_segments_active` 2→4, `secondary_motion_hz` 0.0→2.94, `expression_blendshapes` 1→4).
- `AF-007` (`31613d9`) | `missionary.embrace` & `director.cpp` (`c2_cabin`) | `doll_score`: **25.0 → 95.0** (+70.0), fails: **15 → 1** (expanded 1-frame pose to 25-frame breathing embrace layer; added 2 Hz handheld camera micro-sway in `native/src/game/director.cpp`).

## Scene 3: `old_flames` ("Old Flames")
- `AF-004` (`48e7ddf`) | `cowgirl.in` (`c3_hotel`) | `doll_score`: **35.0 → 95.0** (+60.0), fails: **13 → 1** (`bone_engagement` 4→108, `clavicle_active` False→True, `neck_counter_rotated` False→True, `arc_deviation` 0.14→0.038).

## Scene 4: `neighbours` ("Neighbours")
- `AF-005` (`3da15a9`) | `spoon.in` (`c4_bedroom`) | `doll_score`: **35.0 → 95.0** (+60.0), fails: **13 → 1** (`bone_engagement` 3→106, `secondary_motion_hz` 0.0→2.94, `finger_articulation` False→True, `lipcorner_offset` False→True).

## Scene 5: `salt_and_sun` ("Salt and Sun")
- `AF-006` (`dbcbdba`) | `oral.in` (`c3_terrace`) | `doll_score`: **35.0 → 95.0** (+60.0), fails: **13 → 1** (`bone_engagement` 4→108, `jaw_independent` False→True, `finger_articulation` False→True, `saccade_rate` 0.0→7.5).

## Cross-Runtime Parity & Release Beauty Sweep (`Pass 5 / Step 7`)
- `AF-008` (`2a42edb`) | `all` | Verified cross-runtime parity (`tests/test_anim_parity.py`, `audit/parity/parity_report.json`) with `0.000m` skeleton/pose drift and generated Cycles beauty stills (`audit/<scene>/beauty.png`) across all 5 story scenes.
