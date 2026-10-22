# Status / continuity log

Newest first. Any session continuing the work: read `AGENTS.md`, then this file.

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
