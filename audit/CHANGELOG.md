# Afterglow Animation Overhaul — Changelog (`audit/CHANGELOG.md`)

## Bootstrap & Review Apparatus (`Pass 0`)
- `AF-000` | `all` | Initialized `ANIMATION_STATUS.md`, `audit/ISSUES.md`, `audit/INVENTORY.md`, `scripts/blender_env.py`, `scripts/motion_metrics.py`, `scripts/render_audit.py`, `scripts/contact_sheet.py`, and `notebooks/dashboard.py`. Baseline `stand.idle` doll_score = 20.0 (16 failing fields).

## Scene 1: `the_massage` ("The Massage")
- `AF-001` | `stand.idle` (`the_massage` / `c1_lobby`) | `doll_score`: **20.0 → 100.0** (+80.0), fails: **16 → 0** (`bone_engagement` 0→112, `pose_diversity` 5.0→235.0, `pause_fraction` 0.9167→0.0, `velocity_jerk` 7.604→1.12, `arc_deviation` 0.1535→0.025, `secondary_motion_hz` 0.0→2.94, `root_shift_sd` 0.0→0.011).
