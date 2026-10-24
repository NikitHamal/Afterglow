# CONTINUITY — read this first

Short, current, honest. Newest state at the top. Deeper history: `docs/STATUS.md`; contracts: `AGENTS.md`,
`docs/NATIVE_ENGINE.md`, `docs/NATIVE_CONTENT.md`, `docs/STORY_ASSETS.md`.

The product is the native game in `native/` (C++17 / raylib, built only on GitHub CI). The web game in the repo
root is legacy. Adults only (see `AGENTS.md`).

## Branch `agent-models-hud` (2026-10-24, Agent B) — models + HUD pass (main is owned by another agent)

User asks (2026-10-24): eyes read whitish; hair very poor; Marcus red around the lips; more expressive faces;
no deformation in any pose (Blender or game); HUD buttons/panels/window break on every push → fix globally, modular;
2-3 new best models (adults only); show every model/shot on the molab board.

Diagnosed on molab (main @5a1905a, harness 1280x720 / 1080p): pale grey irises; Vivian's chignon reads as a flat cap with
a grey card streak; Marcus' hair shell shows a bright whorl at the crown and his lower face is orange-red; story
close-ups have a white halo around the actors (naive DOF mixes the blurred foreground into the background); QA shots
never run with FLAG_WINDOW_HIGHDPI (harness uses flags 0), so Windows display scaling bugs never show in QA.

**Round 1 done (2026-10-24, pushed, CI dispatch on 86dbaf6 green; 26b726d dispatched):**
- HUD/window (A): cause = raylib 5.5 FLAG_WINDOW_HIGHDPI only holds until the first resize (laptop-fit at start,
  maximise, borderless, F11) → UI 1.25-2x too big, clicks beside buttons; QA never set the flag. `app/window.*` = one
  coordinate space (no HiDPI flag on Windows/Linux, Retina on macOS), linear UI scale, `ui/layout.*`, scale scopes,
  text fit/ellipsis, `game_hud.cpp`, story UI fit, QA overflow guard (0 findings at 10 sizes + 125/150/200 % after a
  resize). Notes `docs/HUD_NOTES.md`; matrix runner `task/tools/uimatrix.sh`; UI shots `tools/qa/ui_shots.txt` (actors=0).
- Look (B): depth-aware DOF (no halo), eyes (no 3x milky env veil; lid/lash shade, catchlight, limbal-ring iris
  textures `tools/model/native/eye_tex.py`), Marcus lower face rebuilt from the cheek tone (`tools/model/male/face_tone.py`),
  strand hair shading, Marcus scalp-hugging shell with soft hairline (`hair_shell.py`), faces alive (blinks, lid follow,
  listening warmth, emphasis — `scene/facelife.h`). Notes `docs/LOOK_NOTES.md`.
- Still weak: Vivian's chignon (cap look, thin hairline, ribbon tendrils), Marcus' neck colour seam.

**Round 2 (in flight):** (C) deformation-proof skinning (DQS, runtime joint ranges after every layer, stress-pose
harness) + more expression morphs; (D) Vivian hair rebuild + 2-3 new models on the proven rigs (MH face targets on
the s4 basemesh, proxies refit, same skeleton/clips/outfits) + a model roster in Customize.

Tracks: (A) HUD/window: harness `--hidpi`, UI layout layer, overflow-proof widgets, UI shot list;
(B) look: depth-aware DOF, eyes, hair shading, Marcus face; then (C) expressions + deformation-proof skinning,
(D) new models built on the proven rigs. Notes per track: `docs/HUD_NOTES.md`, `docs/LOOK_NOTES.md`.

molab (this session): https://sb-11332fbca0c30924.sb.molab.run/ — board notebook = status + tabs of shots from
`/root/board/{status,shots/<section>}`; toolchain in `/root/agb` (Blender 4.2.3 + MPFB + MH CC0), repo `/root/Afterglow`,
build `/root/build`. Helpers (AgentSky task folder `task/tools/`, need `~/.molab_token`): `mo.py` (run code),
`mjob.py NAME CMD` (detached job + poll), `msync.py REPO REMOTE_DIR` (overlay native/src, shaders, tools/qa),
`mbuild.py SRC BUILD`, `mshots.py BUILD LIST OUTDIR RES`, `fetch.py GLOB DIR`.

## State (2026-10-23, evening)

**Green:** CI builds Windows / Linux / macOS + nightly on every push (the QA-shots job was removed in `b425acd`;
render shot lists on the molab box instead, see Commands).

**Done (latest first)**
- Kiss + hands (agent pass, branch `agent-kiss`): the kiss lands (neck + head IK onto the meeting point, heads roll
  ~22° so noses pass; lips 2 mm apart standing and seated, were 64 / 110 mm); embrace palms flat on the partner's
  real chest / waist (anchors from trunk-weighted skin; the old ray hit surfaces inside the body); forearms take 60 %
  of a palm turn (no flat-ribbon wrists); seated couples turn to each other; harness key `near=` (embrace, no kiss).
- Garments (agent pass, branch `agent-garments`): outfits follow the skeleton again — the outfit GLBs store the
  Japanese joint names JSON-escaped (`\uXXXX`), cgltf left them undecoded, every garment joint fell back to the
  root and the clothes stayed in bind pose (the real cause of "clothes not on the body"); clean cuts along hems /
  cuffs / necklines with corners kept; underwear never drawn under a worn outer layer; cloth shading (sheen BRDF,
  satin anisotropy, finished hems, garment insides in shadow, albedo capped for whites).
- `9d0d7c9` garment fit system (`native/src/scene/fit.*`): covered skin not drawn, smooth borders, drape.
- `ccef91b` embrace hands on the facing side; `c51dbb3` director side fix; `ad90f38` places kit + `cabin_living`.
- `b425acd` CI without the QA job; `a97d631` customise fixes; `cc4137c` this file.

**Half done**
- Marcus: 3 painted tones (others tinted), no `age_up` / `age_dn` (his Age slider shows COMING SOON by design).

**Open (content + engine)** — the ⏳ list in `docs/STORY_ASSETS.md`: spa_suite (4 scenes), villa_terrace (3),
apartment_hall (3), hotel_ballroom, hotel_terrace, villa_beach, apartment_living, the singles; robes / slip / open
shirt; story clips (`stand.embrace`, `kiss.stand`, `dance.slow`, ...); more hairstyles; Marcus v3; petite height.

**Not taken by the 2026-10-23 evening session (by choice, not blocked):** authoring sexually explicit material —
new sex-act clips, genital texture / geometry polish, his hands in the positions, undress clips, arousal mechanics,
explicit story text, renders/videos of the acts. They stay open for whoever owns that track.

## In flight (blocked: the molab render box died, HTTP 410, 2026-10-23 ~23:30 UTC)

Round-2 polish was written but could NOT be rendered, so it is not on `main` (patches kept in the AgentSky task
folder, branches in local worktrees):
- `task/agentC/0001..0002*.patch` (branch `agent-staging2`): embrace hands rest on the measured cloth surface (his far
  hand sank ~15 mm into her dress), hands come in between 0.5 and 1 of the embrace; seated kiss: her hand on his
  chest, his on her waist (elbow hints untuned). Verify: old_flames c4_kiss line 1 `near=1` (two, wide), line 2
  `near=0.35`, the_storm c4_close line 1 `kiss=1` (two, wide), salt_and_sun c3_siesta line 3 `kiss=1` (two, kiss).
- `task/agentD/0001*.patch` (branch `agent-garments2`): small holes cut around the body in a garment layer (his
  trouser crotch, her briefs) are closed at load (a border under 30 cm around that no long bone passes through);
  unit-tested, never rendered. `task/agentD/look_tuning_UNTESTED.patch`: bloom from HDR 1.05 (whites stop glowing),
  daylight mood toned down (villa overexposed). `task/agentD/skirt_spec.md`: the evening dress wraps each thigh (reads
  as shorts) — what the outfit builder must change.
- `task/places/0001*.patch` (branch `places-wip`): the `spa_suite` recipe for `build_place.py` (treatment bed on the
  authoring bed's footprint, shoji wall, lantern key, candles) + `tiles` / `paper_screen` textures — written, never
  baked. Bake with `AG_PLACE=spa_suite ... AG_PREVIEW=1`, look at the previews, then post + stage like cabin_living.
- Still open from the passes: gestures point / reach / wave unchecked; strapless tops' 1–2 px dark slivers;
  salt_and_sun siesta: cyan bikini cups show through her back in the two-shot (not the fit weights; cause open).

## Exact next step

Get a render box back (restart the molab notebook; `uvx marimo@latest pair prompt --url <new url> --with-token`),
Blender 4.2.3 + cmake/xvfb/mesa as at the top of `tools/model/native/setup_toolchain.sh`; fresh CMake configure; apply
and render the two round-2 patches above; merge what looks right; then `spa_suite` with `build_place.py`.

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
