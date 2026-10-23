# Status / continuity log

Newest first. Any session continuing the work: read `AGENTS.md`, then this file.

## 2026-10-23 — content lead: Vivian v3, hands that really touch, new acts

Shipped (`native/assets/characters`, see `docs/NATIVE_CONTENT.md`):
- **Vivian v3** — 10 new expressions (bite_lip, bliss, gasp, smirk, pout, orgasm, wink, shy, lids_down/up for gaze),
  full adult body ranges (skinny↔BBW with real fat distribution, small↔very large bust with sag, hips, butt,
  muscle up/down, belly) and an adults-only age morph (25 → 42 → 60 y, built from MakeHuman's adult range only),
  eight skin tones (porcelain, fair, honey, olive, caramel, bronze, deep, ebony), natural genital pigmentation +
  `vag_aroused` + a wetness mask, `presets.json` (Petite … Ebony goddess, Marcus Lean … Heavy).
- **Hands/feet/necks (issue #5)** — every intended hand contact is solved on the real skin (cup + finger wrap,
  fingertip rubbing with the palm off the belly), anatomical joint ranges enforced on every frame (neck, head,
  wrists, ankles), doggy looks at the sheet ahead (no impossible neck extension), feet posed by the sole.
- **Contacts for the engine's touch simulation** in `clips.json → contacts` + soft bones (his scrotum, her breasts)
  in the armature extras.
- New acts: `handjob.in`, `titjob.in` (verified clean). `facesit`, `cuddle`, `kiss.side` are authored but held back.
- **Story assets** (see `docs/STORY_ASSETS.md` wishes): bedrooms on the exact authoring bed
  `locations/{hotel_room,cabin_bedroom,villa_bedroom}`, outfits `characters/outfits/<who>_<style>.glb` (Vivian
  evening_dress / knit_sweater_jeans / sundress_bikini, Marcus suit_open_collar / flannel_jeans / linen_shirt_shorts,
  each outer + under, skin-tight, carrying every body morph), story clips `stand.idle`, `sit.bed`.

Still open on the content side: petite height (needs per-clip retargeting), more hairstyles (long waves, bob,
ponytail), the non-bed story locations (hotel ballroom/terrace/corridor, cabin living room, villa terrace/beach),
loose garments (skirts/robes with cloth bones), stand.embrace / dance.slow / undress clips, standing/lap/side/69/
fingering clips, Marcus v3, the residuals listed under Verification in `docs/NATIVE_CONTENT.md`.

## 2026-10-23 — the native game (C++ / raylib): engine + game lead

The shipping product is now the native game in `native/` (Windows installer, Linux tarball, macOS zip on the
rolling `nightly` release; in-game updater reads `latest.json` there). The web game in the repo root is legacy.
Read `docs/NATIVE_ENGINE.md` (engine + game reference), `docs/NATIVE_CONTENT.md` (assets), `docs/STORY_ASSETS.md`
(story format + assets the content track should build).

Shipped since the first native build:
- **Game** — title, menu (Continue / Stories / Free play / Customize / Settings / Quit), customisation with a live
  turntable (her: 9 body types, 7 skin tones, hair colour, age 21-60 adults only; his: builds, endowment, tone,
  age), settings (display, graphics, lighting with live preview, audio, controls, game), pause, results.
- **Free play** — the night simulation (`game/night.*`): rhythm by drag / scroll / Space, her desires, connection,
  edging, shared climax; acts are data-driven from `clips.json` (Foreplay / Oral / Positions / Solo) with an
  always-visible action bar (Tab acts, Space rhythm, R touch, E kiss, O her lips, L his tongue, X alone, C camera,
  M mood) and number keys.
- **Story mode** — `native/assets/story/*.json`: Old Flames, The Storm, Salt and Sun (7 chapters, 3 endings each);
  VN UI over the live scene (typewriter, name plates, choice cards, log, auto/skip, 6 save slots + autosave, chapter
  select, gallery); intimate chapters hand over to free play with a goal. Dressed characters are hidden until
  outfit assets exist (see `docs/STORY_ASSETS.md`).
- **Look** — lighting moods (Warm evening default, Candlelit night, Bright daylight) + brightness/warmth, camera-side
  fill, baked room lightmap scaled and blended with live light; skin support maps (flush / translucency / sweat
  masks, pore detail); no dust particles; spring-bone hair with the content's colliders.
- **Touch** — body zones from the bind mesh (vulva, breasts), arm IK onto them (fingers resting on the skin,
  curled to conform), finger contact spheres dent soft tissue in the vertex shader, reactions (arch, flinch,
  breath catch, gasp) by zone sensitivity.
- **Cameras** — collision against body capsules / head spheres, the mattress and the headboard zone, room bounds;
  her face camera, first person, touch close-up (QA).
- **Lifecycle** — per-user installer (no UAC, AppMutex, upgrades, uninstall asks about saves), single instance,
  log + crash note, atomic saves, graceful SIGTERM, 8 fps when unfocused, sleep while minimised.
- **Performance** — sleeping frame limiter (60 fps + vsync default), 2048 shadow map with world-space PCF, PCF
  early-out, out-of-range lights skipped, 12 SSAO samples, ranged morph uploads, texture cap per quality, heap
  trim after load, 4K panels shaded at ~1440p; pause skips the 3D pass under the frosted backdrop.

QA: CI renders `tools/qa/game_shots.txt` (every screen, moods, faces, touches, story screens, two autoplay runs
with an audio event log) and `tools/qa/shots.txt` (engine sandbox) on every push; artifacts `qa-shots`.

Next / open:
1. Outfits, story locations and story-only clips from the content track (docs/STORY_ASSETS.md).
2. Content: hand/finger poses in rub clips (the engine IK fixes the hand onto the surface but the clip's
   arm path still decides the approach), the doggy pose's chin lift, genital-area texture polish (issue #4).
3. His hands on her body (cowgirl / doggy grabs) — the touch system supports it, needs per-act targets.
4. Age morphs (`age_up`/`age_dn`) — the slider is ready.


## 2026-10-22 (later) — v2 of both characters, customisation, performance

Shipped (commits `2820e1c`, `73f586f`, `054a4f3`, plus GLB memory disposal):
- **Vivian v2** — anatomical vulva with labia, clitoral hood, vestibule, real vaginal + anal canals,
  per-tone mucosa paint; hair physics (12 verlet chains, body colliders shipped in the GLB,
  `js3d/glbHair.js`); eye bones (gaze with saccades, eyes roll up at climax); breathing lips;
  7 body-shape morphs applied from `G.char.shape`. Details: `docs/STATUS_female.md`.
- **Marcus v2** — natural flaccid rest, `aroused` morph driven by the game (erect in sex poses,
  soft at intro/idle, refractory after finishing), realistic scrotum with sag bone, short fitted hair,
  subtler veins. Details: `docs/STATUS_male.md`.
- **Customise panel** — her presets Slim / Curvy / Voluptuous / Athletic + sliders; his presets
  Lean / Fit / Muscular / Dad bod, tone, muscle/weight/belly/length/girth.
- **Performance** — 2D +37% fps headless (half-res rim lights, bloom every other frame, HUD diffing,
  adaptive resolution); 3D adaptive pixel ratio, shadow map every other frame, fewer shadow casters,
  lighter room meshes; switching women now frees the previous GLB (only one woman in memory).

Next ideas: landing-strip hair as real short hair, stronger weight/butt morph ranges, hair colliders
that follow body morphs, 2D body layer on its own canvas (saves ~5 full-screen copies).

## 2026-10-22 — realistic characters

Shipped:
- Full visual/audio/feel overhaul of 2D + 3D (room, lighting, post-processing, HUD, character
  select, audio mix, ambience, haptics) — commit "Visual, audio and feel overhaul".
- Removed `hentai/` reference art and the Goat-chan preset (childlike proportions).
- **Vivian** (`assets/vivian/vivian.glb`): realistic woman ~42, mixed heritage, curvy; PBR skin in
  honey / caramel / deep (swapped by the skin-tone slider); expression morphs driven by the game
  (blink timer, moans from voice events, pleasure, orgasm, oral, kiss, afterglow smile); breast
  spring; procedural long layered hair; her own hand-target offsets. Default character.
- **Marcus** (`assets/marcus/marcus.glb`, `js3d/glbHim.js`): realistic man ~40 replacing the
  procedural male's looks (procedural rig still drives him); tone + body/penis morph hooks
  `glbHimSetTone(tone)`, `glbHimSetShape(name, 0..1)`.
- Only the selected GLB downloads now (previously every model, ~250 MB, loaded in the background).

In flight / next (user requests):
1. Female genital anatomy: real geometry (labia, visible vaginal opening), better texture.
2. Male: flaccid by default, erection morph driven by arousal; more realistic genital geometry.
3. Hair: per-strand-chain physics bones + collision so hair never passes through the body when the
   head turns; more realistic cards.
4. Expressions: more range, eye bones (gaze, eyes rolling up at climax), asymmetry.
5. Body variants: shape morphs for both (`body_*`) + customize-panel sliders and preset variants.
6. Performance / memory pass over the whole game.

Known issues:
- Headless SwiftShader can't upload some large textures when several GLBs are loaded (renders black);
  real GPUs are fine. Use `himtex=1024` for headless shots.
- `scratch/` holds old dev probes; not part of the game.
