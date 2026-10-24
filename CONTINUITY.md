# CONTINUITY — read this first

Short, current, honest. Newest state at the top. Deeper history: `docs/STATUS.md`; contracts: `AGENTS.md`,
`docs/NATIVE_ENGINE.md`, `docs/NATIVE_CONTENT.md`, `docs/STORY_ASSETS.md`. Track notes: `docs/HUD_NOTES.md`,
`docs/LOOK_NOTES.md`, `docs/DEFORM_NOTES.md`, `docs/MODELS_NOTES.md`, `docs/ANIM_NOTES.md`, `docs/ENGINE_NOTES.md`.

The product is the native game in `native/` (C++17 / raylib, built only on GitHub CI). The web game in the repo
root is legacy. Adults only (see `AGENTS.md`).

## Where work happens (2026-10-24 evening)

**`main` is the only working branch.** The places agent that owned `main` stopped; Agent B took `main` over and merged
into it `agent-models-hud` (HUD/window, look, deformation, models — rounds 1-3 below) and `agent-modular-models` (another
seat's rig contract: `characters/models.json`, load-time contract check, animator/skin data guards, offline validators
`tools/model/validate_{model,manifest}.py`, browser preview rail `tools/model/preview/`). Those two branches are done;
do not push to them. Pull + rebase on `origin/main` before every push; commit small verified units.

Merge decisions (agent-modular-models): its crude total-angle clamp in `Animator::sanitizePose` was dropped (it fought
the anatomical `JointLimits` from round 2, e.g. the kiss head roll); the data gate (non-finite / degenerate channel →
rest, quaternion renormalise, scale and translation sanity) runs after `JointLimits` (`Character::update`). A rig
contract break is logged, never fatal (it used to unload the character → invisible). `models.json` = rig contract per
role body (+ the preview rail's slider presets); `roster.json` = the selectable faces (Mei, Amara, Daniel, ...).

## Done (2026-10-24, all rendered on molab; board tabs show before/after)

- **HUD / window** (round 1): cause = raylib 5.5's FLAG_WINDOW_HIGHDPI only holds until the first resize (laptop fit at
  start, maximise, borderless, F11) → UI 1.25-2x too big, clicks beside buttons; QA never set the flag.
  `app/window.*` = one coordinate space (no HiDPI flag on Windows/Linux, Retina on macOS); linear UI scale; `ui/layout.*`;
  scale scopes; text fit/ellipsis; `game_hud.cpp`; story UI fits; QA overflow guard: 0 findings at 10 sizes and at
  125/150/200 % after a resize (`tools/qa/ui_shots.txt`, `actors=0`; matrix runner `task/tools/uimatrix.sh`).
- **Look** (round 1): depth-aware DOF (no halo round the actors), eyes (no milky env veil, lid/lash shade, catchlight,
  limbal-ring irises `tools/model/native/eye_tex.py`), Marcus' lower face rebuilt from the cheek tone (`face_tone.py`),
  strand hair shading, Marcus' scalp-hugging hair shell (`hair_shell.py`), faces alive (`scene/facelife.h`).
- **Deformation** (round 2): dual-quaternion skinning for every skinned mesh (`b5da2c5`), anatomical joint ranges on
  every frame after every layer (`35c9ffb`, `anim/rom.*`), stress-pose QA (`tools/qa/deform_shots.txt`), Preserve Volume
  in Blender + weights check, Marcus' under-chin, 11 new Marcus expressions (`7a9db4e`).
- **Models** (round 2): roster + identity packs + Customize "Model" row; hair kit `tools/model/hair/`; Vivian chignon v2;
  new adult models Mei (31), Amara (34), Daniel (34) on the proven bodies (every clip/outfit/contact works for them).
- **Round 3 pushed so far:** `92a0468` soft hair (strand cores opaque, the rest blended order-independently, card sides
  feathered); `9175e74` clip authoring with anatomical arm/leg IK + strict limb metrics (rigkit; clips NOT re-baked yet);
  `b4a868e` QA orbit camera round the couple (`orbit=yaw:,pitch:,dist:`) + skin debug views (`skindbg=1..11`);
  `ae4c43c` Marcus: no red band down the neck in any tone.
- **Story places** (the places agent, before it stopped): spa_suite, apartment_hall / living / balcony / bedroom,
  villa_terrace, cabin_living (see `docs/STORY_ASSETS.md` ✅ list).

## In flight — round 3 strict pass (user: "the kiss is still deformed ... the side pose of the model is deformed too ...
very strict passes and polishes")

Two helpers stopped mid-unit (one API outage, one turn limit). Their uncommitted work is saved in the AgentSky task
folder: `task/wip/anim_r3_uncommitted.patch` (+ `anim_untracked/`, base `9175e74`) and
`task/wip/engine_r3_uncommitted.patch` (+ `engine_untracked/`, base `ae4c43c`); molab state under `/root/agb/anim3`.
- (G) animation: re-bake EVERY clip with the fixed rigkit, author `stand.embrace`, `kiss.stand`, `kiss.sit`, re-author
  `stand.idle` / `sit.bed` (0 furniture hits), splice the animations into the CURRENT GLBs (`splice_anims.mjs`, meshes,
  morphs, hair untouched — skeletons match at 0.000 mm), regenerate clips.json, map story kiss/embrace to the new clips.
- (H) engine: kiss/embrace runtime only fine-tunes the authored clips (lips 0-2 mm, noses pass, no face interpenetration,
  hands on the partner inside the joint ranges; QA keys `kissik=`), side/profile checks of every model.

## Open after round 3

Model identity polish (Mei's face reads weak, Mei's bob hairline jagged), hair from the side/back on every style,
Marcus' age shapes; story places ⏳ in `docs/STORY_ASSETS.md` (hotel_ballroom, hotel_terrace, cabin_door, villa_beach,
spa_lobby / steam / lounge); smoother garment borders; spa robes / slip / open shirt; talk gestures; `massage.back`.
Not taken by any session so far, by choice: sexually explicit authoring (sex-act clips, genital polish, undress clips,
explicit text, renders of the acts).

## molab render box (this session)

https://sb-11332fbca0c30924.sb.molab.run/ — the board notebook shows status cards + tabs of shots from
`/root/board/{status,shots/<section>}` (sections kiss, anim, models, faces, eyes, hair, deform, hud, before_after).
Toolchain `/root/agb` (Blender 4.2.3 + MPFB + MakeHuman CC0), repo `/root/Afterglow`, builds `/root/build*`. Helpers in
the AgentSky task folder `task/tools/` (need `~/.molab_token`): `mo.py` (run code), `mjob.py NAME CMD` (detached job +
poll), `msync.py REPO REMOTE_DIR`, `mbuild.py SRC BUILD`, `mshots.py BUILD LIST OUTDIR RES`, `fetch.py GLOB DIR`.
If the box dies: new notebook URL → `bash task/tools/molab_setup.sh` there (apt, Blender, MPFB, repo), then rebuild.

## Commands

```bash
# CI status of the last runs (AgentSky: the GitHub connector injects auth)
actl connector call github --url "https://api.github.com/repos/NikitHamal/Afterglow/actions/runs?per_page=5"
# push local commits without git credentials (AgentSky sandbox): replays them through the Git Data API,
# checks every tree against the local one, fast-forward only
git fetch origin && git rebase origin/main && /app/py/.venv/bin/python /data/workspace/task/tools/ghpush.py . main
# (ghpush.py lives in the AgentSky task folder, not in this repo)
# harness (needs a build; on molab: Release build in /root/build, Mesa llvmpipe under Xvfb)
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a -s "-screen 0 1920x1080x24" ./afterglow --shotlist tools/qa/game_shots.txt --outdir qa-out --res 1280x720 --frames 24
./afterglow --shot out.png --scene "screen=customize;tab=her" --res 1366x768
# NOTE: shaders are embedded at CMake configure time: re-run `cmake -S native -B build` after editing native/shaders
# (or symlink build/bin/shaders -> native/shaders; the engine loads that folder first)
# a story place: build (+ Cycles previews), post, stage as locations/<id>/
AG_PLACE=cabin_living AG_ROOM_OUT=/root/agb/places/cabin_living AG_PREVIEW=1 BAKE_SAMPLES=512 blender -b --python tools/model/room/build_place.py
node tools/model/room/post.mjs <dir>/<id>_raw.glb <dir>/<id>.json <dir>/post   # room.glb/json -> native/assets/locations/<id>/<id>.*
# molab notebook board (auto-refresh status, live jobs, CI, shots) + 3D viewer of the places: the notebook at
# https://sb-2af46560b43765d5.sb.molab.run/ (status: /root/board/status.json, shots: /root/board/shots/)
```

Rules that bit before: never more than 2 subagents; commit + push after every unit; heavy renders / Blender on the
molab box, not the agent sandbox; keep this file current.
