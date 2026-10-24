# Look notes — track B (eyes, hair, faces, DOF) on branch `agent-models-hud`

Newest first. Board: molab notebook → tabs eyes / hair / faces / before_after (`/root/board/shots/<tab>/`).

## Done (all verified on molab, before/after on the board)
- **DOF halo** `cd161ba` (`native/shaders/dof.frag`): scatter-as-gather bokeh on the sharp frame, no blurred actor in the room.
- **Eyes** `febccaf` (`pbr.frag` mode 3, `renderer.cpp` head uniforms, `character.cpp` `<id>_eye.png` override,
  `tools/model/native/eye_tex.py`): lid/lash shadow + socket corners in the head frame, no x3 ambient veil, two
  catchlights; iris with limbal ring, sclera capped at ~0.55 linear.
- **Marcus' face** `83ed15f` (`tools/model/male/face_tone.py`, run once on the committed albedos): the MakeHuman
  source's beard box, the red philtrum/lips and the red jaw blotches rebuilt in the low band from the cheeks.
- **Hair shading** `8fb53f5` + this commit: per-strand highlight shift/strength, cards rounded across U with a strand
  wave, coverage-weighted highlights, shells keep a weak primary lobe (no crown ring), COLOR_0 alpha = hair coverage,
  the scalp cap (`*cap*` material) shaded as the under-layer, card roots darkened at glTF V = 1 (see below).
- **Marcus' hair shell** `67934d8` (`tools/model/male/hair_shell.py`, run once on the committed marcus.glb): 2747-vertex
  scalp shell with a real COLOR_0.a hairline fringe, polar UVs round the crown; replaces the 407-vertex sawtooth.
- **Expressions** `70d46ee` (`native/src/scene/facelife.h`, `actor.cpp`, `game_play.cpp`): natural blinks, lids that
  follow the gaze (head frame), listening warmth, emphasis on stressed words.

## Next (not done)
- Vivian's chignon asset (`tools/model/female/hair_updo.py`): the swept sheets get sparse right at the hairline, so a
  head tilted down shows a band of soft-edge fragments over the forehead; denser root coverage on the first card
  layer would fix it at the source. Atlas orientation: glTF V = 0 is the painted tip (the PNG is stored flipped),
  roots at V = 1 — the docs' "V root (0) -> tip (1)" is the Blender convention.
- Marcus: a pre-existing UV seam down the side of the neck (head island vs body), a tiny notch at the crown pole.
- Eye colours for new models: `eye_tex.py` + a hue shift option.
- Not mine: a story's first harness shot shows the chapter card (stageStoryNode resets `storyCardT_`).

## Commands
```bash
python3 tools/model/native/eye_tex.py native/assets/characters/vivian.glb native/assets/characters/vivian_eye.png
python3 tools/model/native/eye_tex.py native/assets/characters/marcus.glb native/assets/characters/marcus_eye.png --sat 0.9 --gain 0.8
python3 tools/model/male/face_tone.py native/assets/characters            # from the pre-83ed15f albedos only (not idempotent)
python3 tools/model/male/hair_shell.py native/assets/characters/marcus.glb  # from the pre-67934d8 GLB only
# molab: sync + build + shots (AgentSky task/tools); new/changed assets must be copied into <build>/bin/assets too
python3 msync.py <repo> /root/wt_look && python3 mbuild.py /root/wt_look /root/build_look
python3 mshots.py /root/build_look <list> /root/qa/<name> 1920x1080
```
