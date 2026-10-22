# Male / UI / performance lane — status

## Marcus v2 (natural anatomy)
- Rest geometry is now a **flaccid** circumcised penis (~9.8 cm at build scale, ~8.7 cm in game) hanging forward-down on a
  60° bind axis with a natural drape toward the scrotum (clearance checked in the build log, ~6 mm).
- `aroused` morph (0..1): erect shaft (~16.5 cm build / ~14.7 cm game, slight dorsal curve, defined corona, frenulum notch,
  urethral ridge), widened base, slightly swollen pubic skin, scrotum tightening and lifting. Intermediate weights read as
  a natural semi-erection because both states share the bind axis (no rotation baked into the morph).
- `penis_length` / `penis_girth` are erect-state deltas; the game scales them by `0.35 + 0.65·aroused`.
- Scrotum: two-lobed with a raphe, left side lower, weighted to a new `玉袋` bone (gravity sag + thrust swing in game).
- Bones: `竿根/竿中/竿先` now run straight along the bind axis; `竿端` is an unweighted marker at the erect tip
  (child of `竿先`) so the game knows where the tip is when hard. 70 bones total.
- Paint: subtler veins (3 thin dorsal veins, vein relief halved), glans tone closer to his skin, less gloss.
- Hair: the spiky card cut was replaced by a fitted short-hair **shell** (offset scalp, ~9 mm on top, 3 mm sides) with a
  strand-flow texture radiating from the crown and a hairline fringe carried in `COLOR_0.a` (RGB = 1, so the albedo is
  untouched; three r150 multiplies vertex alpha). The optimiser keeps `COLOR_0` on `marcus_hair` only.

### Game side (`js3d/glbHim.js`)
- `glbHimArousal(dt)`: 0 at intro / solo; 1 while thrusting, during oral, at climax; 0.62–0.9 with her arousal while in a
  pose; drifts to 0.42 after 8 s without stimulation; 0.55 during his finish, then a 9 s refractory window (≤0.45, ≤0.82
  if stimulated). Rises at 0.9/s when stimulated (0.3/s otherwise), falls at 0.14/s.
- `竿根`: slerp from a gravity-tilted hang (≤24°) to the procedural shaft aim by smoothstep(arousal). The `aroused`
  morph follows the same value; tip point lerps between the soft tip and the `竿端` marker.
- Tip alignment (sliding him so his tip meets the procedural tip) only engages above arousal 0.6, eased in by 0.9.
- `玉袋` sags toward gravity (≤0.55 rad) and swings with stroke impulses.

## Customise panel (`js/charui.js`, `css/app.css`)
- The dead Goat-chan "GLB attire" section is gone. New **Her body** block: variants Slim / Curvy / Voluptuous / Athletic
  (write `G.char.shape = {body_bust, body_hips, body_waist, body_butt, body_weight, body_muscle, body_belly}` 0..1 and the
  2D `bodyScale` / `breastSize`), plus per-morph sliders (3D realistic women only). The female lane applies `G.char.shape`
  in `glbModel.js` (`glbApplyShape(preset)` is called if defined, else `glbApplyCustom`).
- **His body** block (3D only): variants Lean / Fit / Muscular / Dad bod, tone Honey / Caramel / Deep, sliders Muscle,
  Weight, Belly, Length, Girth → `G.char.him` (read every frame by glbHim.js). Persisted with `lsSaveChar()`.
- `applyPreset()` now keeps `G.char.him` when switching women (it used to wipe his settings).

## Performance
Measured headless (Chromium + SwiftShader; GPU-bound numbers are not meaningful, so 3D was audited structurally):
- 2D: 7.5 → 10.3 fps in the software rasteriser at a fixed resolution (+37%) from half-resolution rims (rims cost ~1.75
  full-screen passes instead of 8), bloom rebuilt every other frame, and write-through HUD updates (no redundant
  style/text writes, cached element lookups). Adaptive canvas resolution (0.55–1× of DPR) on top for slow devices.
- 3D frame before: 236 draw calls, ~429k triangles (main + shadow + post), 38 shadow casters / 180k shadow triangles
  every frame, full-res HalfFloat 4× MSAA target. Changes: shadow map refreshed every other frame; teeth/tongue/eyes/
  lashes/brows no longer cast; sheet grid 36k → 23k tris, headboard 22k → 12k tris; 2× MSAA when the pixel ratio is
  ≥ 1.5 (halves the HDR target); adaptive pixel ratio 0.75–min(2, DPR) from measured frame time (`js3d/perf3d.js`).
- Adaptive resolution is off in dev screenshot modes (`shot=`, `pose=`, `clean=1`, or `adapt=0`).

## Resume
- Pipeline (molab `/root/agb/male`): `male_genitals.py` (from `out/marcus_a.blend`) → `male_paint.py` → `male_hair.py` →
  `male_rig_export.py` → `textures_small.py` → `node /root/agb/node/male_optimize.mjs export/marcus_raw.glb export/marcus.glb tex`.
  Checks: `male_check2.py -- <tag>` (anatomy, neutral skin, arousal 0/0.5/1), `male_look.py -- <tag>` (textured).
- Ideas not done: a small foreskin variant, a few short top cards over the hair shell for silhouette break-up, per-model
  disposal of unused GLBs when switching women (lives in glbModel.js, female lane).
