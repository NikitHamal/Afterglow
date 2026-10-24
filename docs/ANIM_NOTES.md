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

## In flight
- Story clips (positions4.py): `stand.idle` (contrapposto), `sit.bed` (shins clear of the frame: 46 → 0 furniture hits),
  `stand.embrace` (her arms round his neck, cheek on his chest, his round her back: 4 pairs, 0 mm, max step 8.3°),
  `kiss.sit` (0 pairs, lips 3-9 mm), `kiss.stand` (unstable: neck steps up to 42°, lips 4-17 mm) — head solve
  (`solve_heads`: coordinate descent on neck/head angles, skin-vs-skin penalty from per-bone local BVHs + head sample
  points, crania kept apart, glide from the last frame).
- Story mapping (`game_cinema.cpp`): authored clips layered by the embrace / kiss amounts (stand.idle → stand.embrace →
  kiss.stand; sit.bed → kiss.sit); the procedural step, lean, turn and hand goals stand down where they cover it.

## Next
- kiss.stand: stable head solve (her on her toes, a small lateral offset so the heads tilt apart without extra neck
  twist, ROM penalty raised so the clamp never fights the solve).
- Residuals: legsup forearm on her knee (15 pairs, 0.3 mm); humerus twist 92-108° in oral / lick / handjob / titjob
  (his arms resting palm-down beside him; better than the shipped 152-178°); her rubbing hand pressing her own top
  thigh in spoon.rub.

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
