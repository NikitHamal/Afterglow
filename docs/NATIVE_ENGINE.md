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
  src/gfx                   gl.h, shader (Program), texture (+RenderTarget, ShadowMap), model (glTF), renderer
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
                      -> Procedural::apply (breathing, look-at, jiggle springs, hair verlet) -> finish()
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
| `hairTint`, `flush`, `sweat` | linear hair multiplier; arousal flush 0..1; sweat sheen 0..1 (skin shader) |
| `proc` (`ProcParams`) | `breathRate/breathAmp`, `lookAt/lookTarget/lookWeight/eyeWeight/eyeRollUp/saccades`, `hair`, `hairGravity/Stiffness`, `jiggle/jiggleGain`, `floorY` |
| `placement` | world transform of the model root (identity when clips carry placement) |
| `bonePos(name)`, `boneWorld(name)`, `forward()` | queries after `update` (camera targets, FX anchors) |
| `resetSimulation()` | re-seed hair/springs after a teleport (position change) |

Bone names are MMD (`頭`, `首`, `上半身2`, `下半身`, `腕.L`, `手首.R`, `乳親.L`, `目.L`, Marcus `竿根..竿端`, `玉袋`).
Spring bones: `乳親.L/.R`, `玉袋`. Hair chains: any bones named `髪*` (root = parent not `髪`), colliders from node
extras `afterglow.hairColliders` (`{bone, c: bind-space centre, r}`).

## Scene, lights, camera, audio

- `Scene::load()` loads `assets/room/room.glb` (else a procedural placeholder bedroom), then
  `characters/vivian.glb` (1.68 m) and `characters/marcus.glb` (1.83 m). Legacy web GLBs are fitted to real
  height automatically. Room GLB: `KHR_lights_punctual` spot = lamp key light (shadows), directional = moon
  (`*fill*` = fill), points = candles (`*candle*` flicker). Scene extras `afterglow.bedTop/bedCenter/lamp`.
  Baked rooms: `KHR_materials_unlit` materials, or emissive texture on UV1 as lightmap; characters still cast
  dynamic shadows onto them.
- `Lights` can be edited at runtime (`scene.lights.keyIntensity`, `.ambient`, ...).
- `CameraRig`: `setOrbit(target, yaw, pitch, dist, blend)`, `setFirstPerson(&marcus, "頭", offset, blend)`,
  `sway`, `setFov`; mouse: RMB orbit/look, wheel zoom, MMB pan (`inputEnabled` to block over UI).
- `Audio`: `loadAll()` (assets/audio/**: names like `strokes/stroke03`, `vox/moan02`), `play`, `playRandom(prefix)`
  (no immediate repeats, pitch/volume jitter, 3-voice polyphony), `loop(name, vol)` for files under `loops/` or
  `ambience/`, `addProcedural(name, frames, gen)` for synthesized one-shots, `setMasterVolume`, `update()` per frame.

## Renderer settings (`RenderSettings`)

`applyQuality(settings, Quality::Low|Medium|High)` sets shadow resolution (1k/2k/4k), PCF softness, bloom
levels, soft hair pass, internal resolution. Live grade: `exposure`, `warmth` (-1..1), `saturation`,
`contrast`, `vignette`, `grain`, `bloomStrength`, `flash` (climax flash 0..1). `reloadShaders()` (F5 in the
sandbox) recompiles from `shaders/` next to the asset folder when present, else the embedded copies.

## Harness (visual QA)

```
afterglow --shot out.png --scene "pos=missionary;depth=0.6;cam=side" [--frames 45] [--res 1280x720]
afterglow --shotlist tools/qa/shots.txt --outdir qa-out            # lines: name|spec
```
The harness drives any `App` (`reset()`, `applySpec(spec)`, fixed 1/60 s updates, PNG of the last frame).
Sandbox spec keys: `pos`, `depth`, `rub`, `embrace`, `auto`, `aroused`, `face=name:w,...`, `mface=...` (Marcus),
`body=bust:0.6,...`, `mbody=...`, `tone`, `mtone`, `cam=front|side|top|close|wide|fpv|orbit:yaw,pitch,dist[,x,y,z]`,
`char=both|vivian|marcus`, `quality=low|medium|high`, `debug=1` (overlay), `hair=0`, `exposure`, `flush`, `sweat`,
`frames`. A gameplay App should accept `screen=title|customize|play` plus these keys.

CI (`.github/workflows/native.yml`) renders `tools/qa/shots.txt` under Xvfb + Mesa llvmpipe on every push to
`main` and uploads the PNGs as the `qa-shots` artifact; packages go to the rolling `nightly` release.

## Adding things

- Shader: drop `foo.frag` into `native/shaders/` (re-run CMake configure), load with `shaderSource("foo.frag")`.
- Asset: put it under `native/assets/...`; it is copied next to the executable and installed.
- App: implement `ag::App`, construct it in `src/main.cpp` (and in the harness factory) instead of `Sandbox`.
