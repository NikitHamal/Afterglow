# Deformation notes — track C (skinning, joint ranges, stress QA, Marcus' face) on branch `agent-models-hud`

Newest first. Board: molab notebook → tabs **deform** and **faces** (`/root/board/shots/<tab>/`).

## Done (each verified on molab; before/after on the board)
- **Stress-pose QA** `4234bfa` — `anim/stress.*` + harness keys on `screen=customize`: `stress=<pose>` (arms_up,
  arms_cross, elbows, wrists, squat, knee, split, twist, neck, behind; `overreach` = past anatomy on purpose),
  `clip=<name>;clipT=` (an authored pose held), `outfit=<style>;layer=under` (swimwear / underwear),
  `focus=<bone>|full`, `view=yaw:,pitch:,dist:`, `ui=0`, `skin=lbs`, `rom=0`, `romlog=1`. List: `tools/qa/deform_shots.txt`.
- **Dual quaternion skinning** `b5da2c5` — `anim/animator.*` writes 7 texels per joint (matrix, rigid DQ, scale + linear
  flag); `shaders/mesh.vert` blends DQs (antipodality folded onto the strongest influence), linear only where a joint
  scales non-uniformly. Every skinned mesh (body, outfits, hair styles, face parts) goes through it. Cost: 41.3-41.4 s
  vs 42.1-43.4 s linear for 240 frames of a two-character shot (llvmpipe). The full knee no longer pinches.
- **Joint ranges** `35c9ffb` — `anim/rom.*`, applied in `Character::update` after every layer, before skinning. Range =
  anatomical table ∪ (the character's own clips' envelope + 9°), soft limits (6°), hinges for elbows / knees (axis =
  principal axis of the clips' swing). Authored motion is untouched (24 stress shots: only an over-pointed foot moved).
- **Marcus under-chin / throat** `5f430f4` — `tools/model/male/neck_tone.py` (3D-selected, frequency-separated tone fix,
  all three painted tones; lips unchanged).
- **Marcus expressions** `7a9db4e` — `tools/model/native/expr_units.py` + `expr_apply.py` (MakeHuman expression units on
  the identity recipe's base, spliced as sparse targets): smile, laugh, smirk, brow_up, frown, gasp, bite_lip, wink,
  tender, lids_down, lids_up. Driven in `scene/actor.cpp` (story: by what a face has) and `game_play.cpp` (night).
- **Blender like the engine** `3eb17d1` — Preserve Volume on every pipeline armature; `tools/model/native/weights_check.py`.

## Findings worth knowing
- **Authored arms carry the humerus twist.** Several clips reach their hand orientation with the upper arm twisted
  100-175° and the elbow bent against the hinge (Vivian: missionary.in, titjob.in, spoon.*, revcowgirl.*, handjob.in;
  Marcus: cowgirl.*, doggy.*, sit.bed). Anatomical elbow flexion is the negative hinge direction (all-fours, arms
  overhead, standing confirm it). DQS keeps those arms round; the real fix is in `tools/model/anim/rigkit.py`'s arm IK:
  pick the elbow side anatomically and split the hand's twist between humerus (±90°) and forearm (±85°), then re-bake.
  The runtime ranges include these clips' envelopes on purpose (they never change authored motion).
- **Story IK makes impossible arms.** With `romlog=1`: salt_and_sun c3_siesta (seated kiss) folds her wrist 152° and
  bends the elbow 140° sideways to put her hand on his chest; the ranges now hold the joints, so the hand no longer
  reaches him. Next: make the embrace / seated-kiss arm IK (`scene/actor.cpp`, `scene/touch.cpp`) hinge-aware so the
  contact is met inside the ranges (aim the humerus so the hinge axis is normal to the shoulder-elbow-wrist plane).
- **Marcus' "neck seam" is not a UV seam**: albedo, normal map, ORM, skin mask and vertex normals are continuous across
  every neck seam (checked pair by pair). What remains in warm close-ups is the jaw's cast shadow and the flush mask's
  falloff on the side of the neck.

## Commands
```bash
# molab (AgentSky task/tools): sync, build, shots
python3 msync.py /data/src/<clone> /root/wt_deform && python3 mbuild.py /root/wt_deform /root/build_deform
python3 mshots.py /root/build_deform tools/qa/deform_shots.txt /root/qa/deform 1280x720          # add ;skin=lbs / ;rom=0 to compare
# expressions for a base (Blender + MPFB on molab), then splice into its GLB
blender -b --python tools/model/native/expr_units.py -- marcus /root/agb/deform/marcus_expr.npz
blender -b /root/agb/male/out/marcus_game.blend --python tools/model/native/expr_apply.py -- marcus /root/agb/deform/marcus_expr.npz native/assets/characters/marcus.glb out.glb
python3 tools/model/male/neck_tone.py native/assets/characters        # from pre-5f430f4 albedos only
python3 tools/model/native/weights_check.py native/assets/characters/*.glb native/assets/characters/outfits/*.glb
```
