# Look notes — track B (eyes, hair, faces, DOF) on branch `agent-models-hud`

Newest first. Board: molab notebook → tabs eyes / hair / faces / before_after (`/root/board/shots/<tab>/`).

## Done
- **DOF halo** (`native/shaders/dof.frag`): scatter-as-gather bokeh on the sharp frame, no blurred actor in the room.
- **Eyes** (`pbr.frag` mode 3, `renderer.cpp` head uniforms, `character.cpp` `<id>_eye.png` override,
  `tools/model/native/eye_tex.py`): lid/lash shadow + socket corners in the head frame, no x3 ambient veil, two
  catchlights; iris with limbal ring, sclera capped at ~0.55 linear.
- **Marcus' face** (`tools/model/male/face_tone.py`, run on the shipped albedos in place): the MakeHuman source's
  beard box (darker, edged at the nose base) and the red philtrum/lips/jaw blotches are rebuilt in the low band from
  the cheeks; grain kept. Island from the GLB's UV triangles (interleaved buffer: honour byteStride).

## In flight / next
- Hair: Vivian's tendril cards read as flat grey ribbons (one uniform highlight on a flat card), the chignon as a
  painted cap (normals bent onto one sphere); Marcus' shell: bright crown whorl (radial tangent field) and a sawtooth
  hairline (the engine never read the GLB's `COLOR_0.a` fringe).
- Expressions (engine-side, existing morphs).
- Known, not mine: a story's first shot in a harness list shows the chapter card (stageStoryNode resets
  `storyCardT_` after the spec set it to 99) — put a warm-up shot first.

## Commands
```bash
python3 tools/model/native/eye_tex.py native/assets/characters/vivian.glb native/assets/characters/vivian_eye.png
python3 tools/model/native/eye_tex.py native/assets/characters/marcus.glb native/assets/characters/marcus_eye.png --sat 0.9 --gain 0.8
python3 tools/model/male/face_tone.py native/assets/characters            # from the committed originals only (not idempotent)
# molab: sync + build + shots (AgentSky task/tools); new/changed assets must be copied into <build>/bin/assets too
python3 msync.py <repo> /root/wt_look && python3 mbuild.py /root/wt_look /root/build_look
python3 mshots.py /root/build_look <list> /root/qa/<name> 1920x1080
```
