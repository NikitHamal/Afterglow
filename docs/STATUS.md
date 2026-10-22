# Status / continuity log

Newest first. Any session continuing the work: read `AGENTS.md`, then this file.

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
