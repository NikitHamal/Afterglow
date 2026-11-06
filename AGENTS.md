# AGENTS.md — Afterglow

Read this first, then `docs/STATUS.md` (what is done, what is in flight, how to resume).

## What this project is

Afterglow is an **adults-only (18+) interactive erotic game** for the browser: a 2D canvas game
(`index.html`) and a Three.js 3D version (`3d.html`) of the same night between two consenting
adult partners. It is fictional, local, and contains explicit sexual content by design — working
on it means writing rendering, animation, audio, UI and 3D-character code for that content.

Content rules that every contributor (human or AI) follows:

- **Every character is an adult and must read as an adult** — face, body proportions, voice lines,
  UI copy. No childlike proportions, no school/age-play framing, no "youthful" coding.
- The old `hentai/` reference folder and the **Goat-chan** preset were removed for this reason
  (childlike reference art / chibi proportions). Do not reintroduce them or anything like them.
- Characters are depicted as consenting partners; keep it that way in copy and mechanics.

## Layout

| Path | What |
|---|---|
| `index.html`, `js/` | 2D game. `core.js` (state `G`, `CFG`), `audio.js` + `ambience.js` (WebAudio mix), `chars.js` (presets), `input.js`, `mechanics.js` (rhythm/pleasure/stamina), `gfx.js` + `atmos.js` (room, post), `side.js` + `poseside.js` (side view), `fpv.js`, `poses.js`, `oral.js`, `charui.js` (character select + customize), `juice.js` (popups, haptics, help, pause), `app.js` (loop/HUD) |
| `3d.html`, `js3d/` | 3D game. `engine3d.js` (renderer, camera rig), `room3d.js` + `roomtex3d.js` (procedural bedroom), `post3d.js` (bloom/grade/vignette/grain), `chars3d.js` (procedural fallback rigs `her3`/`him3`), `poses3d.js`, `anim3d.js` (IK, contact), `fx3d.js` (particles), `glbModel.js` (realistic/GLB women), `glbHim.js` (realistic man), `app3d.js` (boot + tick) |
| `assets/vivian/` | Vivian — realistic PBR woman (~42): `vivian.glb` + `skin_{honey,caramel,deep}.jpg` |
| `assets/marcus/` | Marcus — realistic PBR man (~40): `marcus.glb` + skin/genital tone JPEGs |
| `assets/*.glb` | Older third-party MMD-style models (Kiyoko, Anime) |
| `tools/model/` | The Blender + MPFB pipeline that builds Vivian and Marcus (see `docs/MODEL_PIPELINE.md`) |
| `audio/`, `assets/Shinlalala's Lewd Sound Pack/` | Sound pack (CC BY-SA 4.0, credited in `index.html`) |
| `scratch/` | Old dev probes/screenshot scripts (not shipped) |

## How the 3D characters work

- `her3` / `him3` (procedural rigs in `chars3d.js`) are always the **motion source**: poses,
  thrust, IK hands, contact all run on them. Realistic GLB models are skinned meshes that copy
  those joints every frame; the procedural meshes are hidden while a GLB is active.
- GLB women use MMD bone names (`上半身`, `腕.L`, `ひざ.R`, `乳親.L`, fingers `人指０.L`…) and the
  bone frames of the old Goat-chan rig — `GLB_RIG_MAP` in `glbModel.js` depends on those axes.
  New models must keep that convention (the pipeline does it automatically).
- Per-model behaviour lives in `GLB_REGISTRY` (`glbModel.js`): `pbr`, `face` (expression morphs),
  `jiggle`, `tones` (albedo swap by `G.char.skinTone`), `land` (hand-target offsets).
- Only the selected model is downloaded; others load on demand from the character picker.

## Conventions

- Classic scripts in global scope (no `import`/`export` in `js/` or `js3d/`); load order is the
  `<script>` order in the HTML files. Never redeclare another file's top-level names.
- Keep files under ~600 lines; split helpers into companion modules.
- `node --check <file>` on every touched JS file. Preserve CRLF where a file already uses it.
- Every `X.save()` has a matching `X.restore()`; cache gradients/offscreen layers (60 fps budget).
- Verify visually: serve the repo (`python3 -m http.server 5500`) and screenshot with headless
  Chromium/Playwright. URL params: `?pose=N&char=<preset>&play=1&solo=1&clean=1` skip the intro;
  `post=0` disables post-processing; `himtex=1024` caps Marcus' textures for SwiftShader.
- GLB post-processing (`tools/model/*/optimize.mjs`): quantize OK; **do not** use gltf-transform
  `sparse()` (empty morph accessors crash three r150), and strip `COLOR_*` attributes (GLTFLoader
  multiplies them into the albedo).

## Pushing

The repo is `NikitHamal/Afterglow`, branch `main`. Commit small, verified steps.
