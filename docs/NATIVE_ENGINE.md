# Afterglow native engine — reference

C++17 on raylib 5.5 (window, input, audio, text, 2D/UI) with the 3D path written directly against OpenGL 3.3
(raylib's statically linked glad: `src/gfx/gl.h`). glTF via cgltf (compiled inside raylib), images via a
private stb_image. Contract and asset conventions: `docs/NATIVE_PLAN.md`.

## Layout

```
native/
  CMakeLists.txt            FetchContent raylib 5.5 + raygui 4.0, shader embedding, asset copy
  cmake/copy_assets.cmake   native/assets -> <bin>/assets (+ legacy web assets until content lands)
  shaders/*.vert|*.frag     GLSL 330 — embedded at configure time (CONFIGURE_DEPENDS)
  installer/                Inno Setup script, icon, 18+ licence/credits, Windows resources
  src/core                  mathx (column-major V3/Quat/M4), json, paths, spec (harness specs), log
  src/gfx                   gl.h, shader (Program), texture (+RenderTarget, ShadowMap), model (glTF), renderer,
                            particles (soft billboards, additive/alpha, floor collision)
  src/anim                  animator (clips, layers, masks, skinning texture, morphs), procedural
  src/scene                 character, scene (+ lights), camera, procroom (placeholder bedroom)
  src/audio                 audio (named samples, variants, loops, procedural buffers)
  src/app                   app.h (App interface), sandbox (F1 overlay), harness (screenshots)
```
Output: `native/build/bin/Afterglow(.exe)` with `assets/` next to it.

## Frame flow

```
app->update(dt):  Character::setLayer/setMorph/setBody/... ; Scene::update(dt)
                   -> per character: Animator::begin -> applyClip(layers) -> computeGlobals(root)
                      -> bone offsets -> Procedural::apply (breathing, look-at, jiggle springs, hair spring
                         bones) -> finish()
                         (joint matrices -> RGBA32F bone texture, dirty morphs -> VBO updates)
app->render(w,h): CameraRig::update -> Renderer::render(scene, cam, w, h, time) -> raylib 2D/UI on top
```
`Renderer::render` leaves GL in the state raylib's 2D batch expects, so `DrawText`, raygui and any custom UI
can be drawn right after it inside `BeginDrawing/EndDrawing`.

## Character (`src/scene/character.h`) — what gameplay drives

| Call | Meaning |
|---|---|
| `setLayer(i, "missionary.in", depth, 1.f)` | layer `i` (0..5) samples clip at normalized time 0..1 (for `.in` clips t = penetration depth; for loops t = phase) |
| `setLayer(1, "missionary.rub", phase, w, "arm.R")` | masked layer; masks: `all`, `arm.L/R`, `arms`, `hand.L/R`, `head`, `legs`, `upper`, `lower`, `bones:a,b` (subtrees) |
| `setMorph("blink", 1)` / `setMorph(name, -1)` | override a morph on every mesh that has it / release back to the animated value |
| `setBody("bust", -1..1)` | drives `bust_up`/`bust_dn` pairs (falls back to legacy `body_bust`) |
| `setTone("honey"|"caramel"|"deep")` | albedo swap from sibling JPEGs `<id>_skin_<tone>.jpg` (and `<id>_genital_<tone>.jpg`) |
| `hairTint`, `flush`, `sweat` | linear hair multiplier (content hair carries its colour in `baseColorFactor`, so the default is white; only the legacy grey hair gets a chestnut tint); arousal flush 0..1; sweat sheen 0..1 (skin shader) |
| `setBoneOffset("腕.L", euler)` / `clearBoneOffsets()` | additive local rotations applied after the clips (pose tweaks, QA flex test) |
| `proc` (`ProcParams`) | `breathRate/breathAmp`, `lookAt/lookTarget/lookWeight/eyeWeight/eyeRollUp/saccades`, `hair`, `hairGravity/Stiffness`, `jiggle/jiggleGain`, `floorY` |
| `placement` | world transform of the model root (identity when clips carry placement) |
| `bonePos(name)`, `boneWorld(name)`, `forward()` | queries after `update` (camera targets, FX anchors) |
| `headAxes(center, faceFwd, crownUp)`, `hairCenter()` | world head frame (face cameras, gaze) and skull centre (hair shading) |
| `resetSimulation()` | re-seed hair/springs after a teleport (position change) |

Bone names are MMD (`頭`, `首`, `上半身2`, `下半身`, `腕.L`, `手首.R`, `乳親.L`, `目.L`, Marcus `竿根..竿端`, `玉袋`).
Spring bones: `乳親.L/.R`, `玉袋`. Hair chains: any bones named `髪*` (root = parent not `髪`), colliders from node
extras `afterglow.hairColliders` (`{bone, c: bind-space centre, r}`, JSON string or object).

Hair follows the VRM spring-bone model at a fixed 90 Hz: each joint is pulled toward its *animated* direction
relative to its *simulated* parent (stiffness 1.8 at the root falling to 0.55 at the tip, x `hairStiffness`), plus
gravity (0.9 x `hairGravity`), 35 % drag, length kept, pushed out of the colliders (+1.2 cm) and above `floorY`
(the mattress). A teleport (reset or a head jump > 35 cm) settles 0.5 s in the new pose instead of popping.
Look-at clamps: neck 0.25 rad at 35 % weight, head 0.55 rad, fading out for targets more than ~75 deg off the
face; content clips carry the authored gaze, so the sandbox uses `lookWeight` 0.45 there (0.9 when alone).

## Scene, lights, camera, audio

- `Scene::load()` loads `assets/room/room.glb` (else a procedural placeholder bedroom built to the content
  authoring bed: centre (0, 0.604, -1.22), mattress 1.80 x 2.00 m, headboard toward -Z, lamp on the left
  nightstand, candles on the right, window + moon on the right wall), reads `room/room.json` when present
  (`bed.top|center|size`, `lamp.pos`, or flat `bedTop/bedCenter/bedSize/lamp`), then
  `characters/vivian.glb` (1.68 m) and `characters/marcus.glb` (1.83 m). Legacy web GLBs are fitted to real
  height automatically. Room GLB: `KHR_lights_punctual` spot = lamp key light (shadows), directional = moon
  (`*fill*` = fill), points = candles (`*candle*` flicker). Scene extras `afterglow.bedTop/bedCenter/lamp`.
  Baked rooms: `KHR_materials_unlit` materials, or emissive texture on UV1 as lightmap; characters still cast
  dynamic shadows onto them.
- `Lights` can be edited at runtime (`scene.lights.keyIntensity`, `.ambient`, ...).
- `Scene::particles` (`src/gfx/particles.h`): `emit(EmitParams)` (position/velocity jitter, gravity, drag, life,
  size and colour over life, additive or alpha, velocity stretch, floor collision); the scene keeps lamp-beam
  dust motes alive on its own.
- `CameraRig`: `setOrbit(target, yaw, pitch, dist, blend)` (yaw 0 = camera on +Z looking toward -Z),
  `setFirstPerson(&marcus, "頭", offset, blend)`, `setFirstPersonLocal(&marcus, "頭", offsetLocal, forwardLocal,
  blend)` (eye + look axis in the bone's frame, from `clips.json`), `follow(target)` (track a moving target
  without touching yaw/pitch/dist), `sway`, `setFov` (orbit 38 deg; first person blends to 58 deg); mouse: RMB
  orbit/look, wheel zoom, MMB pan (`inputEnabled` to block over UI).
- `Audio`: `loadAll()` (assets/audio/**: names like `strokes/stroke03`, `vox/moan02`), `play`, `playRandom(prefix)`
  (no immediate repeats, pitch/volume jitter, 3-voice polyphony), `loop(name, vol)` for files under `loops/` or
  `ambience/`, `addProcedural(name, frames, gen)` for synthesized one-shots, `setMasterVolume`, `update()` per frame.

## Content metadata (`characters/clips.json`)

The sandbox (and any gameplay App) reads it next to the GLBs:

- `clips.<name>.layerBones = {char: mask}`: masks for layered clips (`.rub`, `.embrace`), mask syntax as above.
- `clips.<name>.marcusHidden`: hide Marcus for that clip (solo clips).
- `clips.<name>.morphs = {char: {morph: rule}}`: rule is a number or `"smoothstep(a, b, depth)"`; other rule
  strings are left to gameplay. Marcus' `length`/`girth` body sliders scale by `0.35 + 0.65 * aroused`.
- `clips.<pos>.in.cameras.orbit_<preset> = {target, yaw, pitch, distance}` (degrees). Content yaw is measured
  from the head side, so the engine uses `yaw + 180`. `cameras.fpv = {character, bone, offset_local,
  forward_local}` feeds `setFirstPersonLocal`.
- Presets without content data: `close` (bone-relative face shot from the front-side, on the side away from the
  partner, kept above the mattress, follows the head), `portrait` (whole subject from its front three-quarter),
  `wide`. Presets resolve after the next pose update, so they always see the new pose.

## Renderer settings (`RenderSettings`)

Pipeline: shadow map (lamp spot, frustum fitted to the bed, Poisson PCF) -> depth prepass -> half-res SSAO +
depth-aware blur -> forward HDR (skinned PBR; skin wrap/second lobe/translucency + flush/sweat; Kajiya-Kay
hair with normals bent around the skull centre, R + fibre-tinted TRT lobes, strand variation; eye glint;
baked or lit room) -> soft hair edges (blended) -> particles -> bloom -> ACES + grade -> FXAA.

`applyQuality(settings, Quality::Low|Medium|High)` sets shadow resolution (1k/2k/4k), PCF softness, SSAO
(off/10/16 samples), bloom levels, soft hair pass, internal resolution (0.8/1/1). Live grade: `exposure`, `warmth` (-1..1), `saturation`,
`contrast`, `vignette`, `grain`, `bloomStrength`, `flash` (climax flash 0..1). `reloadShaders()` (F5 in the
sandbox) recompiles from `shaders/` next to the asset folder when present, else the embedded copies.

## The game (`src/game`, `src/ui`, `src/app`)

`afterglow` starts the game; `afterglow --sandbox` the engine sandbox. Flow: loading (progress between load
steps) -> title over the live bedroom -> main menu (Continue / New night / Customize / Settings / Quit) ->
customisation (live turntable preview: her bust/hips/waist/bottom/weight/tone/belly via the `_up/_dn` morphs,
skin tone, hair colour, presets; his muscle/weight/belly, endowment, tone, presets) / settings (display,
graphics, audio, controls, game) / the night -> pause -> results.

- `ui::Ui` — immediate-mode kit drawn over the 3D frame: embedded OFL fonts (`native/res/fonts`, rasterised at the
  exact pixel size, HiDPI aware), `ui_rect.frag` SDF shapes (rounded, gradients, borders, soft shadows) and
  frosted glass sampling `Renderer::blurTexture()` (enabled by `renderer.uiBlur`), widgets with hover/press
  animation and keyboard focus (arrows/Enter/Esc, Tab/Q/E for tabs). Layouts are written in 1920x1080 units (`u()`).
- `Night` — the simulation (pure C++, balance-tested headless): strokes are detected from the depth signal
  (tempo, amplitude, energy); her arousal follows rhythm match x position x freshness x her favourite, touch and
  kisses, scaled by connection; his follows energy and her pleasure and cools off when you slow down (edging).
  Desires (faster, slower, deeper, gentler, touch, kiss, change position) time out; meeting them builds a
  streak and connection. Her climax keeps the night going; his ends it (together = big payoff, too soon = penalty).
- Presentation (`game_play.cpp`): clip layers per position (`<pos>.in` at the played depth, `.rub`/`.embrace`
  layers, solo idle + rub), expressions (blink, pleasure, moan pulses on strokes, smile, kiss, eye roll at
  climax), flush/sweat/breathing/jiggle by state, gaze, cinematic camera cuts between the clips.json presets,
  first person (V), climax flash/bloom/slow motion, afterglow pull-back.
- Audio (`game_audio.cpp`): strokes/plaps/wet by impact and her arousal, moans/breaths by arousal (breaths and UI
  sounds are synthesised), generated room ambience + a slow pad on an audio-thread stream.
- Persistence: `<user>/settings.json`, `<user>/profile.json` (looks, stats, the night to continue), written
  atomically; autosave every 20 s in play. User folder: `%APPDATA%\Afterglow`, `~/.config/afterglow`,
  `~/Library/Application Support/Afterglow`; logs in `logs/afterglow.log` (+ `.prev.log`, crash notes).
- Lifecycle (`main.cpp`, `app/platform`): single instance (Windows mutex `AfterglowGameMutex`, shared with the
  installer's AppMutex; a lock file elsewhere), pause + 8 fps when unfocused, sleep while minimised, F11 /
  Alt+Enter borderless toggle, graceful shutdown (save, stop audio, free GL).
- Updater (`app/updater`): CI builds `0.<run>.0` and publishes `latest.json` (`version`, `notes`, `page`,
  `assets.<windows|windows-portable|linux|macos> = {url, sha256, size}`) with the nightly release. The game
  checks it on start (setting), shows a card in the menu, downloads with progress (WinHTTP / `curl`), verifies
  the SHA-256, then runs `Afterglow-Setup.exe /VERYSILENT /SUPPRESSMSGBOXES /CLOSEAPPLICATIONS /NORESTART
  /RELAUNCH` and quits (Linux/macOS open the release page). Dev builds (`0.0.0-dev`) never update.
- Installer (`native/installer/afterglow.iss`): per-user (`%LOCALAPPDATA%\Programs\Afterglow`, no UAC), in-place
  upgrades, Start menu + optional desktop shortcut, uninstaller that clears logs/caches and asks whether to
  keep saves. Linux tarballs carry `afterglow.desktop` + `install-desktop-entry.sh`.
- Memory/CPU: textures capped per quality (1K Low, 2K Medium, full on High; tone swaps replace the embedded
  albedo), CPU mesh copies released after upload (morph meshes keep base + sparse deltas), sparse CPU morphing,
  frosted-glass blur only while a menu shows, heap trimmed after loading. F1 shows fps, frame time, draws, RAM
  and GPU memory. Measured (Mesa llvmpipe, same scene): peak RSS 877 -> 727 MB; Low quality 343 MB RSS with
  100 MB of textures vs 340 MB on High; simulation + animation + morphing ~1-1.5 ms CPU per frame.
- Pause: once the frosted backdrop covers the frozen scene the 3D pass is skipped (only the cached blur is
  shown). F12 saves a screenshot (UI included) to `<user>/screenshots`.
- Loading: a model's embedded images decode on worker threads while the main thread keeps pumping window events
  and redrawing the loader (`Model::loadPump`); first start picks Low for software GL, Medium for integrated GPUs.
- Audio levels (measured offline): generated room tone ~-35 dBFS, pad ~-30 dBFS; voice samples normalised with
  per-sample gains in `Game::voice`.
- Installer extras: offers to remove an all-users copy left by early nightlies; `licenses/OFL-*.txt` ship with
  the embedded fonts; SIGTERM/SIGINT/SIGHUP quit gracefully on Linux/macOS.

## Harness (visual QA)

```
afterglow --shot out.png --scene "pos=missionary;depth=0.6;cam=side" [--frames 45] [--res 1280x720]
afterglow --shotlist tools/qa/game_shots.txt --outdir qa-out       # game screens; lines: name|spec
afterglow --sandbox --shotlist tools/qa/shots.txt --outdir qa-out  # engine sandbox shots
```
The harness drives any `App` (`reset()`, `applySpec(spec)`, fixed 1/60 s updates, PNG of the last frame).
Sandbox spec keys: `pos`, `depth`, `rub`, `embrace`, `auto`, `aroused`, `face=name:w,...`, `mface=...` (Marcus),
`body=bust:0.6,...`, `mbody=...`, `tone`, `mtone`,
`cam=front|side|top|close|portrait|wide|fpv|orbit:yaw,pitch,dist[,x,y,z]` (orbit values in radians, engine yaw),
`char=both|vivian|marcus`, `quality=low|medium|high`, `debug=1` (overlay), `hair=0`, `exposure`, `flush`, `sweat`,
`flex=0..1` (bone-offset deformation test), `frames` (sandbox). Game specs: `screen=title|menu|customize|settings|play|pause|results`, `tab=her|him`,
`settab`, `pos`, `depth`, `her`, `him`, `connection`, `desire`, `rub`, `embrace`, `cam=side|front|top|fpv`, `bar=1`,
`climax=her|him|together`, `result=together|soon`, `intro=1`, `body`, `mbody`, `tone`, `mtone`, `hair`, `quality`,
`hints`, `t` (seconds into the screen).

CI (`.github/workflows/native.yml`) renders `tools/qa/shots.txt` under Xvfb + Mesa llvmpipe on every push to
`main` and uploads the PNGs as the `qa-shots` artifact; packages go to the rolling `nightly` release.

## Adding things

- Shader: drop `foo.frag` into `native/shaders/` (re-run CMake configure), load with `shaderSource("foo.frag")`.
- Asset: put it under `native/assets/...`; it is copied next to the executable and installed.
- App: implement `ag::App`, construct it in `src/main.cpp` (and in the harness factory) instead of `Sandbox`.
