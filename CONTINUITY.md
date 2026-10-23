# CONTINUITY — read this first

Short, current, honest. Newest state at the top. Deeper history: `docs/STATUS.md`; contracts: `AGENTS.md`,
`docs/NATIVE_ENGINE.md`, `docs/NATIVE_CONTENT.md`, `docs/STORY_ASSETS.md`.

The product is the native game in `native/` (C++17 / raylib, built only on GitHub CI). The web game in the repo
root is legacy. Adults only (see `AGENTS.md`).

## State (2026-10-23)

**Green:** `main` @ `cac059e` — CI run 39974443889: build-windows / build-linux / build-macos / qa-screenshots /
nightly-release all `success`; rolling `nightly` release updated.

**Done (latest first)**
- Customise (her): `Tone` slider −1..1 (the content's `muscle_dn`; Skinny's −0.3 is kept and its chip lights);
  preset chips sized to their names in balanced, justified rows (13 presets → 5/4/4); footer clear at 1280×720.
  Verified on molab: build OK, harness shots of both tabs at 1280×720 / 1366×768 / 1920×1080.
- `cac059e` story film fixes (garment colours, cameras for seated couples, film framing, story acts only).
- `ffbb849` content M5–M7 wired into the game: 8 skin tones (porcelain … ebony) by name, body types from
  `characters/presets.json`, adult age shapes (`age_dn` = 25 y, base 42 y, `age_up` = 60 y; 21–25 read as 25),
  v3 faces in play, soft bones + hair swing limits from the armature, clips.json contact dents.
- `6d930d4` / `2520052` five staged, voiced stories; story mode as a film (outfits, locations, lip sync, acting,
  directed cameras). `cf0bb66` Kokoro voices (CI bot).
- Content M5–M7: Vivian v3, outfits (3 + 3), `hotel_room` / `cabin_bedroom` / `villa_bedroom`, `stand.idle`, `sit.bed`.

**Half done**
- Marcus: 3 painted tones (honey / caramel / deep); the other 5 are tints of them. No `age_up` / `age_dn` in
  `marcus.glb`, so his Age slider still shows the COMING SOON tag (by design until the morph ships).

**Open (content + engine)** — the ⏳ list in `docs/STORY_ASSETS.md` (non-bed locations, robes / slip / open shirt,
smoother garment borders, `stand.embrace`, `kiss.stand`, `dance.slow`, `walk.in`, `lean.rail`, `sit.couch`), more
hairstyles (long waves, bob, ponytail), Marcus v3 (age shapes + 8 painted tones), petite height (per-clip
retargeting), the residuals under Verification in `docs/NATIVE_CONTENT.md`.

**Not taken by the 2026-10-23 evening session (by choice, not blocked):** authoring sexually explicit material —
new sex-act clips, genital texture / geometry polish, his hands in the positions, undress clips, arousal mechanics,
explicit story text. They stay open for whoever owns that track.

## In flight

1. Story locations: a kit for non-bed places on top of `tools/model/room/build_room.py`, most-used first —
   `cabin_living` (5 scenes), `spa_suite` (4), `villa_terrace` (3), `apartment_hall` (3), then `hotel_ballroom`,
   `hotel_terrace`, `villa_beach`, `apartment_living`, the singles. Blender 4.2.3 on molab.

## Exact next step

Install Blender 4.2.3 on molab (`tools/model/native/setup_toolchain.sh` has the URL), build `cabin_living` with the
room kit, bake, `post.mjs`, render the harness story shots that use it, push.

## Commands

```bash
# CI status of the last runs (AgentSky: the GitHub connector injects auth)
actl connector call github --url "https://api.github.com/repos/NikitHamal/Afterglow/actions/runs?per_page=5"
# push local commits without git credentials (AgentSky sandbox): replays them through the Git Data API,
# checks every tree against the local one, fast-forward only
git fetch origin && git rebase origin/main && /app/py/.venv/bin/python /data/workspace/task/tools/ghpush.py . main
# (ghpush.py lives in the AgentSky task folder, not in this repo)
# harness (needs a build): game screens / one shot
afterglow --shotlist tools/qa/game_shots.txt --outdir qa-out
afterglow --shot out.png --scene "screen=customize;tab=her" --res 1366x768
```

Rules that bit before: never more than 2 subagents; commit + push after every unit; heavy renders / Blender on the
molab box, not the agent sandbox; keep this file current.
