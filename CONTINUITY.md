# CONTINUITY — read this first

Short, current, honest. Newest state at the top. Deeper history: `docs/STATUS.md`; contracts: `AGENTS.md`,
`docs/NATIVE_ENGINE.md`, `docs/NATIVE_CONTENT.md`, `docs/STORY_ASSETS.md`.

The product is the native game in `native/` (C++17 / raylib, built only on GitHub CI). The web game in the repo
root is legacy. Adults only (see `AGENTS.md`).

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

## In flight

1. Verify the merged kiss + garment work on molab (fresh CMake configure: shaders are embedded at configure time,
   `cmake --build` alone keeps old shaders), push.

## Exact next step

`spa_suite` (The Massage, 4 scenes) with `tools/model/room/build_place.py`: a treatment bed on the authoring bed's
footprint (0.604 m top, 1.8 x 2.0 m, centre (0, 0.604, -1.22) engine) so every bed clip works there; then
villa_terrace, apartment_hall, hotel_ballroom.

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
