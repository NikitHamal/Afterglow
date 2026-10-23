# CONTINUITY — read this first

Short, current, honest. Newest state at the top. Deeper history: `docs/STATUS.md`; contracts: `AGENTS.md`,
`docs/NATIVE_ENGINE.md`, `docs/NATIVE_CONTENT.md`, `docs/STORY_ASSETS.md`.

The product is the native game in `native/` (C++17 / raylib, built only on GitHub CI). The web game in the repo
root is legacy. Adults only (see `AGENTS.md`).

## State (2026-10-23, evening)

**Green:** CI builds Windows / Linux / macOS + nightly on every push (the QA-shots job was removed in `b425acd`;
render shot lists on the molab box instead, see Commands).

**Done (latest first)**
- `ad90f38` story places kit (`tools/model/room/kit.py`, `build_place.py`) + `cabin_living` (The Storm, 5 scenes);
  'fire' lights flicker; missing places fall back to a built sibling of their family first.
- `c51dbb3` director: face-to-face couples shot from the open side (they were shot from behind the bed); the side is
  picked again when the pose changes (seated couples were shot from behind).
- `b425acd` CI without the QA job (it took ~55 min).
- `a97d631` customise: her muscle −1..1 (`muscle_dn`), preset chips sized to their names in balanced rows.
- `cc4137c` this file. Before that: `ffbb849` 8 skin tones / presets.json / adult age shapes, story film, M5–M7 content.

**Half done**
- Marcus: 3 painted tones (others tinted), no `age_up` / `age_dn` (his Age slider shows COMING SOON by design).

**Open (content + engine)** — the ⏳ list in `docs/STORY_ASSETS.md`: spa_suite (4 scenes), villa_terrace (3),
apartment_hall (3), hotel_ballroom, hotel_terrace, villa_beach, apartment_living, the singles; robes / slip / open
shirt; story clips (`stand.embrace`, `kiss.stand`, `dance.slow`, ...); more hairstyles; Marcus v3; petite height.

**Not taken by the 2026-10-23 evening session (by choice, not blocked):** authoring sexually explicit material —
new sex-act clips, genital texture / geometry polish, his hands in the positions, undress clips, arousal mechanics,
explicit story text, renders/videos of the acts. They stay open for whoever owns that track.

## In flight

1. Garment fit (engine, `native/src/scene/fit.*`): garments are offset copies of the skin (6–7 mm) that sink into the
   body under skinning (seated: the sweater front up to 6 mm inside, measured in Blender), so the skin showed through.
   Now, per garment at load: skin covered by an opaque layer is not drawn while that layer is worn; an under layer
   hides under the outer one; garments trim along a smoothed copy of their border (no triangle staircase at hems and
   necklines). Lace covers nothing. Automatic for any future garment / accessory.

## Exact next step

Check the molab renders of `fit_*` (seated two-shot, standing two-shot, kiss, hotel) — no skin through cloth, no holes
at borders, smooth necklines; then commit + push the fit system, then `spa_suite` with `build_place.py`.

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
# a story place: build (+ Cycles previews), post, stage as locations/<id>/
AG_PLACE=cabin_living AG_ROOM_OUT=/root/agb/places/cabin_living AG_PREVIEW=1 BAKE_SAMPLES=512 blender -b --python tools/model/room/build_place.py
node tools/model/room/post.mjs <dir>/<id>_raw.glb <dir>/<id>.json <dir>/post   # room.glb/json -> native/assets/locations/<id>/<id>.*
# molab notebook board (auto-refresh status, live jobs, CI, shots) + 3D viewer of the places: the notebook at
# https://sb-2af46560b43765d5.sb.molab.run/ (status: /root/board/status.json, shots: /root/board/shots/)
```

Rules that bit before: never more than 2 subagents; commit + push after every unit; heavy renders / Blender on the
molab box, not the agent sandbox; keep this file current.
