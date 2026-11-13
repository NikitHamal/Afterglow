# Afterglow Animation Corpus Inventory (`audit/INVENTORY.md`)

## 1. Character Rigs (`native/assets/characters/{vivian,marcus}.glb`)
- **Vivian (`VivianRig` / `vivian_body`)**:
  - **Core Spine & Pelvis**: `root` (root), `center`, `groove`, `pelvis` (pelvis), `spine` (lower spine), `chest` (upper chest), `neck` (neck), `head` (head), `eyes` / `eye.L` / `eye.R` (eyes).
  - **Clavicles & Arms**: `shoulder.L/R` (clavicle), `upper_arm.L/R` (upper arm), `elbow.L/R` (forearm), `wrist.L/R` (wrist).
  - **Fingers**: `thumb0..2.L/R` (thumb), `index1..3.L/R` (index), `middle1..3.L/R` (middle), `ring1..3.L/R` (ring), `pinky1..3.L/R` (pinky).
  - **Legs & Feet**: `thigh.L/R` (thigh), `knee.L/R` (shin), `ankle.L/R` (ankle/foot), `toe.L/R` (toes).
  - **Soft Tissue & Secondary**: `breast.L/R`, `breast_tip.L/R` (breasts), hair chains (`hair*`).
- **Marcus (`MarcusRig` / `marcus_body`)**:
  - Shares full MMD humanoid hierarchy (`pelvis`, `spine`, `chest`, `neck`, `head`, `shoulder.L/R`, `upper_arm.L/R`, `elbow.L/R`, `wrist.L/R`, 5-finger chains, `thigh.L/R`, `knee.L/R`, `ankle.L/R`, `toe.L/R`, `eye.L/R`) plus anatomy chain (`shaft_base`, `shaft_mid`, `shaft_tip`, `shaft_end`, `scrotum`).

## 2. Expressions & Shape Keys
- Authored via `tools/model/native/{expr_units.py,expr_apply.py}` and driven at runtime by `Animator::setMorphOverride`, `native/src/scene/actor.cpp`, `native/src/scene/facelife.h`, and `native/src/game/voice.cpp`.
- Morph targets cover eyelids/blink, brows, mouth openness/visemes, lip corners (smile/frown), cheeks, and anatomy morphs (`vag_open`, `aroused`).

## 3. Animation Clips (`tools/model/anim/positions*.py` & GLB tracks)
| Clip Name | Source File | Frames | Kind | Tag | Used In |
|---|---|---|---|---|---|
| `stand.idle` | `positions4.py` | 49 | `loop` | `STORY` / `IDLE` | All 5 stories (standing dialogue beats) |
| `stand.embrace` | `positions4.py` | 49 | `loop` | `STORY` / `INTIMACY` | All 5 stories (embrace / closeness beats) |
| `kiss.stand` | `positions4.py` | 49 | `loop` | `STORY` / `INTIMACY` | All 5 stories (standing kiss beats) |
| `sit.bed` | `positions4.py` | 49 | `loop` | `STORY` / `IDLE` | All 5 stories (bedside dialogue beats) |
| `kiss.sit` | `positions4.py` | 49 | `loop` | `STORY` / `INTIMACY` | All 5 stories (seated kiss beats) |
| `missionary.*`, `cowgirl.*`, `revcowgirl.*`, `doggy.*`, `spoon.*`, `lotus.*`, `stand.*` | `positions.py`, `positions2.py` | varies | `depth` / `loop` | `INTIMACY` | Story climax/intimacy scenes & Freeplay |
| `oral.*`, `lick.*`, `handjob.*`, `titjob.*`, `facesit.*`, `cuddle.*`, `kiss.*`, `solo.*` | `positions2.py`, `positions3.py` | varies | `depth` / `loop` | `INTIMACY` | Story foreplay/intimacy & Freeplay |

## 4. Story Scenes (`native/assets/story/*.json`)
1. `the_massage` ("The Massage" — `spa_suite` / `hotel_room`)
2. `the_storm` ("The Storm" — `cabin_living` / `cabin_bedroom`)
3. `old_flames` ("Old Flames" — `hotel_room` / `apartment_living`)
4. `neighbours` ("Neighbours" — `apartment_hall` / `apartment_balcony` / `apartment_bedroom`)
5. `salt_and_sun` ("Salt and Sun" — `villa_terrace` / `villa_bedroom`)

## 5. Runtime Players
1. **Desktop & Android Native Engine (`native/src/`)**:
   - `ag::Animator` (`native/src/anim/animator.cpp`): Quaternion/translation channel sampling, masked layer blending, dual-quaternion GPU skinning.
   - `ag::Actor` / `ag::Scene` (`native/src/scene/actor.cpp`, `scene.cpp`): Story actor pose blending, `Blinker` (`facelife.h`), gaze tracking, breathing, lip-sync (`voice.cpp`).
   - `ag::Night` (`native/src/game/night.cpp`): Intimacy state machine, crossfades, procedural pelvis/chest/head/hand layering (`procedural.cpp`).
2. **Web Preview Runtime (`tools/model/preview/viewer.js`)**:
   - Three.js GLTF/GLB loader and `THREE.AnimationMixer` player.
