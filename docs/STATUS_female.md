# Female track (Vivian) — lane notes

Pipeline lives in `tools/model/female/` (see `docs/MODEL_PIPELINE.md`). Order:
`build5.py` → `hair.py` → `rig_export.py` → `textures_small.py` → `node optimize.mjs raw.glb vivian.glb tex/`.
`build5.py` writes `out/vivian_s4.blend` (old stage name kept so the later scripts run unchanged).

## v2 (2026-10-22)

**Anatomy** — `vulva.py` (exec'd by build5): the crotch is subdivided 3x (+1 fine pass on the inner
vulva and anus, ~1.2 mm edges) and sculpted from analytic profiles laid out along the body midline arc:
labia majora, vulvar cleft, asymmetric ruffled labia minora, clitoral hood + glans, vestibule, urethral
dimple, perineal raphe, pleated anus. Real openings: a 34 mm vaginal canal and a 13 mm anal canal made
from stepped insets pushed along the anatomical axis, closed at the bottom, UV'd into free texture space.
Per-vertex masks (`v_muc, v_min, v_maj, v_clit, v_anus, v_pub, v_cav`) are baked and drive the per-tone
paint (mucosa, pigmentation, canal darkening, landing strip). Shape keys are carried across the topology
change (deltas re-applied; new vertices get IDW-blended deltas).
Gotchas found: BMesh vertex wrappers do not survive `subdivide_edges` as dict keys, and creating a
custom-data layer invalidates live wrappers — use int/float layers created up front. Blender exits 0 even
when the `--python` script raises: check the logs, don't trust `&&`.

**Body-shape morphs** — `body_bust, body_hips, body_waist, body_butt, body_weight, body_muscle, body_belly`:
deltas between variant MPFB base meshes (head/neck masked, pelvis re-centred). Game: `glbShape3` applies
`G.char.shape = {body_bust: 0..1, …}` every frame for any GLB with `body_*` morphs (missing keys = 0).

**Hair physics** — `hair.py` groups strands into 12 chains (k-means on root + tip), writes chain joints;
cards are weighted along each chain (two nearest chains, smooth seams) and twisted per card.
`rig_export.py` adds bones `髪.<n>.a…d` + tip leaf `髪.<n>.z` on `頭`, and body colliders (head, neck,
upper-torso quadrants, breasts, shoulders) as glTF extras (`afterglow.hairColliders`).
Game: `js3d/glbHair.js` (verlet chains with style memory, gravity, damping, collider push-out, teleport
reset); tune live via `GLB_HAIR_TUNE`.

**Expressions** — eye bones `目.L/目.R` (eyes reweighted to them). `glbEyes3` aims them at the camera with
saccades, clamped to a natural range, rolling up near/at orgasm. `glbFace3`: breathing lips that quicken
with arousal; each moan picks an open "ah" or a rounded "oh".

## Known issues / next
- Small dark specks at the refined-region boundary near the inner thighs (geometry); glossy seams at the
  outer edge of the refined region in close-ups.
- Landing strip reads as sparse dots; wants a directional short-hair pattern.
- Hair colliders are fixed at rest shape; strong body morphs (weight/muscle) can let tips graze the back.
