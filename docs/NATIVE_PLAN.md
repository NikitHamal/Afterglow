# Afterglow Native — architecture and contracts

The browser version (index.html / 3d.html) is now legacy. The product is a **native, installable PC game**
built only on GitHub CI. Characters: **Vivian** and **Marcus** only (adults; see AGENTS.md content rules).

## Stack

- C++17, CMake ≥ 3.21. **raylib 5.5** (window, input, OpenGL 3.3 via rlgl, textures, shaders, render
  targets, audio via miniaudio, TTF text) fetched with FetchContent. **cgltf** + **stb_image** (shipped
  inside raylib `src/external`). No other runtime dependencies. Static runtime on Windows (`/MT`).
- Targets: Windows x64 (MSVC) → Inno Setup installer + portable zip; Linux x64 → tar.gz; macOS arm64 → zip.
- Code lives in `native/`: `src/core`, `src/gfx`, `src/anim`, `src/game`, `src/ui`, `src/audio`,
  `shaders/` (GLSL 330, embedded at build time), `assets/` (installed next to the executable).

## World

- Meters, **Y up**, glTF conventions. Characters at real scale: Vivian 1.68 m, Marcus 1.83 m.
- The room (`assets/room/room.glb`) defines the bed; mattress top height is published in the room
  GLB's scene extras (`afterglow.bedTop`, meters). Characters' clips are authored in that same world
  space, so placement is baked into the clips (no runtime fitting, no floating).

## Character GLBs (`assets/characters/vivian.glb`, `marcus.glb`)

- Skinned, PBR (baseColor, normal, ORM), alphaMode MASK for hair/brows/lashes, real scale, MMD bone names
  (as in the web build) + hair chains `髪.<n>.a..z` (Vivian) + eyes `目.L/目.R` + Marcus `竿根/竿中/竿先/竿端/玉袋`.
- Morph targets (names in mesh `extras.targetNames`):
  - faces: `blink, moan, pleasure, oral, kiss, smile` (Vivian), `blink, effort, pleasure, moan, kiss` (Marcus)
  - body (bidirectional pairs, slider −1..+1 → negative uses `_dn`, positive `_up`):
    Vivian `bust_up/bust_dn, hips_up/hips_dn, waist_up/waist_dn, butt_up/butt_dn, weight_up/weight_dn, muscle_up, belly_up`;
    Marcus `muscle_up/muscle_dn, weight_up/weight_dn, belly_up, length_up/length_dn, girth_up/girth_dn`
  - anatomy: Vivian `vag_open` (labia part + opening widens to his girth), `anal_open`; Marcus `aroused` (0 soft → 1 erect)
- Alternate skin tones ship as sibling JPEGs `<name>_skin_{honey,caramel,deep}.jpg` (albedo swap).
- Hair collider spheres in node extras `afterglow.hairColliders` (bone + offset + radius).

## Animation clips (same names in both GLBs, authored together in one world)

| Clip | Meaning |
|---|---|
| `<pos>.in` | Thrust axis. **Normalized time t∈[0,1] = penetration depth** (0 tip at entrance, 1 full). Game samples t = depth. |
| `<pos>.rub` | Loop (1 s). Only the rubbing arm/hand bones (`rubHand` in clips.json says whose). Layered over `.in` with a bone mask, time = rub phase. |
| `<pos>.embrace` | (missionary) pose layer: he lowers to kiss her. Blend weight = embrace. |
| `oral.in` | Blowjob, t = depth of her head on his shaft. `lick.in` cunnilingus, t = tongue stroke. |
| `solo.rub` | Vivian alone, loop; `solo.idle` loop. |

Positions `<pos>`: `missionary, legsup, doggy, prone, cowgirl, revcowgirl, spoon`.
Morph weights may be animated in clips (`vag_open` with depth, Marcus `aroused`=1 in sex clips).
`assets/characters/clips.json`: per clip duration, rubHand, camera presets (orbit target/yaw/pitch/dist,
FPV bone + offset), stroke length, and contact notes.

Authoring is done in Blender (tools/model/anim/*) with both armatures in the room; every clip is verified
for: feet/knees/hips resting on the mattress (|gap| < 1 cm), no body-body interpenetration outside the
intended genital/mouth contact (BVH test), hands touching their targets, penetration axis alignment.

## Engine contract (for gameplay code)

- `Assets::load()`; `Character` (model + skeleton + morph weights + material tone/tint)
- `Character::setClip(layer, name, t, weight)`, procedural layers: breathing, look-at, hair verlet, jiggle springs
- `Character::setMorph(name, w)`; `Renderer::draw(scene, camera)`; `Camera` orbit/preset/FPV with smooth blends
- Screenshot harness: `afterglow --shot out.png --scene "<spec>" [--frames N] [--res WxH]` and
  `--shotlist tools/qa/shots.txt` (one `name|spec` per line). Spec keys: `screen=title|customize|play`,
  `pos=`, `depth=`, `rub=`, `embrace=`, `face=name:w,...`, `body=bust:0.6,...`, `tone=`, `cam=preset|orbit:yaw,pitch,dist`.

## CI (`.github/workflows/native.yml`)

Windows/Linux/macOS Release builds (FetchContent cache); Windows job builds the Inno Setup installer; pushes to
`main` update the `nightly` pre-release with all packages. QA shots are rendered on the molab box, not in CI.
