# Animation notes — track G (strict pass on every clip) on `main`

Newest first. Board: molab notebook → tabs **anim** (acts, before/after in the game renderer, underwear) and **kiss**
(story kisses / embrace). Raw numbers: `/root/agb/anim3/verify_*.json` on molab; the table below is from `verify_full4`.

## Done
- **Acts re-baked with an anatomical solver** (unit 1). What the shipped clips did, measured on the shipped GLBs with
  `author.py --from-glb`: elbows bent the mirror way with the humerus wrung 140-178° to fake it (doggy, cowgirl,
  reverse, handjob, titjob, lick, sit.bed), missionary's arms hyperextended, kneeling knees bent 26-61° off their hinge,
  wrists parked at the 85° twist limit, 20-34° wrist pops between frames, 114 / 38 / 31 / 26 / 16 / 14 intersecting
  triangle pairs in legsup / missionary / cowgirl / reverse / reverse.rub / prone, and in spoon both bodies floating
  over the sheet (only the bottom thumbs touched it).

| clip | bad pairs | deepest mm | max step ° | max humerus twist ° | floors her / his mm |
|---|---|---|---|---|---|
| missionary.in | 38 → 0 | 24.3 → 0 | 8.9 → 10.0 | 97 → 92 | −8 / −1 |
| missionary.rub | 0 → 0 | 0 → 0 | 1.3 → 10.0 | 119 → 81 | −8 / −1 |
| legsup.in | 114 → 15 | 26.0 → 0.3 | 16.9 → 4.6 | 119 → 80 | −2.5 / −1.9 |
| doggy.in | 0 → 0 | 0 → 0 | 9.3 → 13.0 | 142 → 46 | −4 / −13 |
| doggy.rub | 0 → 0 | 0 → 0 | 1.5 → 1.6 | 160 → 58 | −4 / −13 |
| prone.in | 14 → 0 | 4.3 → 0 | 2.0 → 2.0 | 62 → 62 | −9 / −2.4 |
| cowgirl.in | 31 → 0 | 16.6 → 0 | 34.1 → 9.0 | 166 → 77 | −2.8 / −8.8 |
| cowgirl.rub | 0 → 0 | 0 → 0 | 7.8 → 0.9 | 152 → 80 | −0.9 / −7.3 |
| revcowgirl.in | 26 → 0 | 7.9 → 0 | 22.9 → 6.9 | 166 → 37 | −1.4 / −5.0 |
| revcowgirl.rub | 16 → 0 | 0 → 0 | 6.9 → 2.5 | 146 → 62 | −2.2 / −4.4 |
| spoon.in / .rub | 0 → 0 | 0 → 0 | 4.6 → 12.0 | 87 → 78 | **floating** → −12.3 / −4.7 |
| oral.in | 0 → 0 | 0 → 0 | 5.8 → 5.8 | 81 → 93 | −6.4 / 0.3 |
| lick.in | 0 → 0 | 0 → 0 | 12.7 → 10.3 | 178 → 108 | −0.3 / −21 |
| handjob.in | 1 → 0 | 0 → 0 | 19.8 → 8.4 | 174 → 93 | −8 / 0.5 |
| titjob.in | 8 → 0 | 0 → 0 | 7.4 → 1.4 | 152 → 93 | −9 / 0.5 |

(legsup's 15 pairs: his forearm resting on the outside of her knee in the calf hold, 0.3 mm.)

- **Solver** (`tools/model/anim/rigkit.py`, `body.py`, `positions*.py`):
  - `ik2`: hinge axes of both bones on the bend-plane normal with the limb's own flexion sense (`FLEX_SIGN`: elbow
    +, knee −), flexion capped (`MAX_FLEX` 150 / 160°). Arms: the authored elbow is kept while the humerus twist is
    anatomical (`ARM_TWIST_MAX` 80°); otherwise the nearest elbow on the circle that is — never deeper in the mattress
    or in either body (capsules: trunk, rib cage, shoulder girdle, breasts, head, legs, the partner's arms), never more
    than 45° from the last keyed frame's elbow, and found by bisection so it glides. `strict_elbow` (set by
    `touch_tips`, a rubbing hand whose forearm passes between the bodies) also rejects an authored elbow deep in a
    capsule.
  - `share_twist`: the forearm takes half of the hand's pronation (unwrapped frame to frame).
  - `Body.leg_aim`: thigh and shin share one hinge (the hip rotates, the knee never bends sideways).
  - `natural_pole`: the 20° pole grid is refined continuously (no 20° elbow hops between frames).
  - `smooth_nearest`: hands aim along a contact normal averaged over a 2-3 cm disc (a BVH's flat face normals made
    cupped hands snap 15-25° from face to face).
  - `hands_to(plant=True)`: a hand bearing weight presses ~4 mm into the mattress; any hand near the sheet is lifted
    out of it by its skinned mesh.
  - `best_of` trials no longer steer each other's continuity memory; spoon scores his layouts on his finished body.
  - spoon: both bodies rest on hip + side + shoulder (arms never count when settling); the bottom knee and foot are
    raised / lowered until they rest lightly on the sheet; her top knee lifted in the rub (room for her hand).
- **Verification** (`author.py`): per-limb metrics (humerus/forearm twist, signed flexion, off-hinge bend, wrist and
  ankle), `--from-glb DIR` verifies the clips shipped in DIR's GLBs, lip gaps for kisses, `AG_FLIPDBG=1` logs every
  joint step over 12°, `AG_NAT` work dir for the whole chain.
- **Splice instead of re-export** (`tools/model/native/splice_anims.mjs`): only animations move into the shipped GLBs
  (faces, expressions, eyes, hair, identity data stay); it refuses when the skeleton's rest transforms differ.
  `glb_diff.mjs` proves it: Vivian byte-identical outside the clips; Marcus' sparse morph targets differ only by
  −0/+0 signs (values identical, max |diff| 0). `validate_model.py` OK for both.
- **QA keys** (play stills): `dress=<her style>,<his style>;layer=under` (underwear, nothing explicit),
  `focus=v:<bone>|m:<bone>` + `view=yaw:,pitch:,dist:` (camera on one joint). Shot list used: 12 acts × depth 0/1 ×
  side/front (`/data/tools/qa_acts.txt` in the AgentSky sandbox; the board sheets `anim/act_<id>.jpg`).

## In flight — story clips authored + verified in Blender, NOT yet in the GLBs (the molab box died: HTTP 410)
Measured in `author.py` (Blender, same verify as the acts) before the box went away:

| clip | bad pairs | deepest mm | max step ° | other |
|---|---|---|---|---|
| stand.idle | 0 | 0 | 0.3 | contrapposto: weight on one leg, free hip dropped, soft free knee, shoulders countering, soft elbows |
| sit.bed | 0 | 0 | 6.2 | furniture hits 46 → 0 (shins hang clear of the frame), his arm round her shoulders |
| stand.embrace | 4 (her head × his neck, 0 mm) | 0.0 | 8.3 | her arms round his neck, cheek resting on his chest, his round her back |
| kiss.stand | 5 | 0.1 | 4.6 | lips 2.5-7.6 mm, heads rolled 20° apart (noses pass), she is on her toes (heels 3.5 cm up) |
| kiss.sit | 0 | 0.0 | 4.6 | lips 3.4-9.5 mm, hips turned in 28°, chests 30° more, knees forward, her hand on his chest, his at her back |

How the heads are solved (`positions4.solve_heads`): coordinate descent on neck/head pitch + yaw of both, each head
rolled 20° to its own right and yawed 5°, lips (or her cheek on his chest) as the goal; skin-vs-skin penalty from
per-bone surfaces in each bone's own frame + ~65 sample points per head (lips excepted); crania kept apart; range
excess penalised (1.0 standing kiss, 0.05 elsewhere); the first frame decides the body lean (best of four, cold),
later frames warm-start from the last one, glide (5e-2) and move at most 6° per angle per frame.

Story mapping (`game_cinema.cpp`, pushed, inert until the clips are in the GLBs — it checks `hasClip`): standing,
stand.idle → stand.embrace (by the embrace amount) → kiss.stand (by the kiss amount) as layers 0/1/2; seated, sit.bed →
kiss.sit; where an authored clip covers it the procedural step, lean, turn and hand goals stand down (the runtime kiss
IK still closes the last millimetres).

## Exact next step
1. A render box: `bash molab_setup.sh` (AgentSky task folder `task/tools/`: apt + Blender 4.2.3 + MPFB + repo), then the
   character pipeline `bash tools/model/native/run_chars.sh both` (needs `/root/agb/goatchan.glb` =
   `git show 1b10cf2:assets/goatchan/goatchan.glb`), then the authoring prep (Commands below: prepare both + room).
2. `author.py -- stand.idle,sit.bed,stand.embrace,kiss.stand,kiss.sit --norender` — expect the table above
   (stand.embrace was last measured before the `tip_toes` no-op fix; re-check it is back to 4 pairs / 0 mm).
3. `render_story.py` sheets (heads + upper, side/front/top/back) → by eye; then export, splice ONLY those five
   clips (`splice_anims.mjs ... stand.idle,sit.bed,stand.embrace,kiss.stand,kiss.sit`), merge their clips.json
   entries, `validate_model.py` both, build, story QA in the game (`screen=story;...;kiss=1` / `near=1`, the
   couple orbit `orbit=yaw:,pitch:,dist:`; old_flames c4_kiss, the_storm c4_close, salt_and_sun c3_siesta seated),
   board tab `kiss`, push.

## Next (after that)
- Residuals: legsup forearm on her knee (15 pairs, 0.3 mm); humerus twist 92-108° in oral / lick / handjob / titjob
  (his arms resting palm-down beside him; better than the shipped 152-178°); her rubbing hand pressing her own top
  thigh in spoon.rub (−30 mm, as shipped).

## Commands
```bash
# molab: prepare both characters + a room (fast bake) into the work dir, then author / verify
export AG_NAT=/root/agb/anim3; B=/root/agb/blender-4.2.3-linux-x64/blender
$B -b /root/agb/out/vivian_game.blend --python tools/model/native/prepare.py -- vivian
$B -b /root/agb/male/out/marcus_game.blend --python tools/model/native/prepare.py -- marcus
AG_ROOM_OUT=$AG_NAT/room BAKE_RES=256 BAKE_SAMPLES=2 $B -b --python tools/model/room/build_room.py
cd tools/model/anim && $B -b --python author.py -- [clip,clip] --norender          # bake + verify -> $AG_NAT/anim.blend, verify.json
$B -b --python author.py -- --norender --from-glb native/assets/characters          # verify the shipped clips
$B -b $AG_NAT/anim.blend --python render_story.py -- <out> kiss.sit:0.5 --views side,front,top --frame heads|upper|body
# export, splice into the shipped GLBs (only the listed clips), merge clips.json, validate
$B -b $AG_NAT/anim.blend --python tools/model/native/export_chars.py
node tools/model/native/splice_anims.mjs native/assets/characters/vivian.glb $AG_NAT/export/vivian_raw.glb out/vivian.glb clip,clip,...
node tools/model/native/glb_diff.mjs native/assets/characters/vivian.glb out/vivian.glb     # only animations may differ
$B -b $AG_NAT/anim.blend --python tools/model/anim/clips_json.py -- clips_all.json             # then merge the clips you ship
python3 tools/model/validate_model.py out/vivian.glb --manifest native/assets/characters/models.json --slot vivian
```
