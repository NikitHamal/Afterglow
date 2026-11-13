# Afterglow — Animation Status & Doll-Factor Overhaul (`SESSION A1`)

## Overview
- **Mission**: Full story-mode and intimacy motion overhaul ("KILL THE DOLL") across body mechanics, timing/curves, secondary physics, facial/eye/lip-sync animation, camera staging, and cross-runtime playback parity.
- **Current Phase**: Step 0 / Step 1 — Toolchain verification, inventory baseline, and review apparatus construction.
- **Active Issue**: `AF-000` (verified & closed) — Created `ANIMATION_STATUS.md`, `audit/ISSUES.md`, and `audit/INVENTORY.md`.

## Corpus Summary
| Category | Assets / Files | Notes |
|---|---|---|
| Character Rigs | `native/assets/characters/vivian.glb`, `native/assets/characters/marcus.glb` | MMD-convention bone hierarchy (`下半身`, `上半身`, `上半身2`, `首`, `頭`, `肩.L/R`, `腕.L/R`, `ひじ.L/R`, `手首.L/R`, fingers `親指/人指/中指/薬指/小指`, legs `足/ひざ/足首`, breasts `乳親.L/R`, shaft `竿根/竿中/竿先/玉袋`). |
| Hair & Outfits | `native/assets/characters/hair/*.glb`, `native/assets/characters/outfits/*.glb` | `bob_mei`, `crop_daniel`, `long_kari`, `puff_amara` + 6 outfit GLBs. |
| Authoring Pipeline | `tools/model/anim/{author.py,body.py,positions.py,positions2.py,positions3.py,positions4.py,rigkit.py,scene.py,clips_json.py}` | Procedural + IK/FK pose solver baking actions `VIV|<clip>` and `MAR|<clip>`. |
| Story Scenes | `native/assets/story/{the_massage,the_storm,old_flames,neighbours,salt_and_sun}.json` | 5 interactive story graphs authored via `tools/story/*.py`. |
| Runtime Players | `native/src/anim/{animator,procedural,rom,stress}.cpp`, `native/src/scene/{actor,night,facelife,oral}.cpp`, `tools/model/preview/viewer.js` | C++ OpenGL/GLES desktop & Android player + Three.js web preview player. |

## Scene Scorecard Summary (Pre vs Post)
| Scene ID | Title | Clips Used | Baseline Doll Score | Current Doll Score | Status |
|---|---|---|---|---|---|
| `the_massage` | The Massage | `stand.idle`, `stand.embrace`, `kiss.stand`, `sit.bed`, `kiss.sit`, intimacy acts | Pending baseline | Pending | In Audit |
| `the_storm` | The Storm | `stand.idle`, `stand.embrace`, `kiss.stand`, `sit.bed`, `kiss.sit`, intimacy acts | Pending baseline | Pending | In Audit |
| `old_flames` | Old Flames | `stand.idle`, `stand.embrace`, `kiss.stand`, `sit.bed`, `kiss.sit`, intimacy acts | Pending baseline | Pending | In Audit |
| `neighbours` | Neighbours | `stand.idle`, `stand.embrace`, `kiss.stand`, `sit.bed`, `kiss.sit`, intimacy acts | Pending baseline | Pending | In Audit |
| `salt_and_sun` | Salt and Sun | `stand.idle`, `stand.embrace`, `kiss.stand`, `sit.bed`, `kiss.sit`, intimacy acts | Pending baseline | Pending | In Audit |

## Ablation & Change Log Summary
- `AF-000`: Initialized `ANIMATION_STATUS.md`, `audit/ISSUES.md`, and `audit/INVENTORY.md` to track the 20-field doll-factor audit and 5-pass motion overhaul.
