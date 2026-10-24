# Engine notes — track H (round 3: hair passes, skin bands, the kiss at runtime, side/profile) — now on `main`

Newest first. Board: molab notebook → tabs hair / kiss / anim (`/root/board/shots/<tab>/`).

## Done
- **Soft hair** (`gfx/hairoit.*`, `shaders/hair_oit.frag`, `pbr.frag` / `depth.frag` hair terms, `renderer.cpp`):
  hair cards draw only their dense strand cores opaque (alpha >= `kHairCore` 0.86, in the colour pass and the depth
  prepass alike); everything softer — the translucent sheet under the strands, tips, wisps, the cap's hairline fade —
  goes to a weighted blended OIT pass (McGuire-Bavoil; GL 3.3: one blend function, colour/weight add, revealage
  multiplies in target 0's alpha) composited over the HDR frame, so overlapping cards average instead of fighting over
  draw order. Card long sides feather out (hair-kit convention: one atlas column per card, u in [col+.03, col+.97]/8),
  per-strand highlight variation settles where strands are sub-pixel, card rounding only where a card is wide on screen,
  cards lean 65 % on the skull sphere (was 50 %). A/B of five variants at 1080p chose this one (broader lobes / 82 %
  sphere smeared the crown highlight). QA: `hairdbg=1` paints soft layers green, strand cores red.
  Board: hair/engine_soft_hair_before_after.

- **Marcus' neck band** `ae4c43c` (`tools/model/native/{rim_repair,tone_chroma,uv_lines,tone_seams}.py`, all three
  painted tones, Daniel uses them): `skindbg=4` (albedo only) proved it painted into the albedo — a dark red rim 8-10
  texels (4K) along the head / neck UV islands (the earlier tone fixes lifted the interior, not the border), a ~1.2 cm
  pink strip under the jaw and below each ear (R/G 1.70 against 1.58 round it), the MakeHuman beard box's straight
  side edges inside the head island (v 0.341 / 0.692). Frequency-separated passes (detail kept): rims extended from the
  interior + padding filled (no mip bleed), strips pulled to the 3D median skin, colour edges blended (chroma only:
  luminance gradients are shading — clamping them lifted the sideburn fade 20 %, reverted), box edges spread into
  ramps on skin columns only, seams matched (median 0.3 %, max 2-4 %). Board faces/marcus_neck_band_*.
- **Kiss and hands inside the joint ranges** (`anim/rom.*` range query + one-bone clamp, `scene/actor.cpp` kissIK,
  `scene/touch.cpp` solveArmRanged): the clamp log (`romlog=1`) showed the story IK producing what the ranges then
  undid — heads 53-68° swing / 77-80° twist, elbows 136-148° off their hinge with ±130-155° twist, wrists folded
  151-158° — so lips and hands missed (seated kiss: lips 67-83 mm apart, faces 12-17 mm into each other).
  * kissIK: cyclic coordinate descent over head / neck / chest with shares 0.5 / 0.55 / 0.35, each joint clamped at
    once, then the roll about the pivot -> lips line (crown upright + kissRoll; lips stay put), neck and chest give
    back what the head's range took. A face-orientation variant (`kissik=3`, deleted) was worse on every number
    (standing lips 4.3-5.3 mm vs 2.0-2.1, misses 3-4 mm vs <= 1, seated 31 mm).
  * arms (world goals: story hands): the elbow bends about its own hinge only (the shoulder -> wrist distance fixes
    the angle, inside [hingeMin, hingeMax]), the arm turns about the shoulder -> wrist line until the elbow points
    along the goal's hint as far as the upper arm's twist range allows, the palm's turn goes to the forearm twist
    inside its range and the wrist takes the rest (clamped). Zone goals (play-mode touches) keep the old solve.
  * seated: the spine takes more of the turn when kissing (lower back 0.4-0.45 · yaw, chest 0.2 · yaw · kiss) and the
    look-at no longer fades with the lean (nothing steps in when seated, the heads kept turning away: mouths 23-26 cm
    apart before the kiss IK at full kiss vs 12 cm at 0.3).
  * numbers (molab, kissqa=1, last frame): standing kisses lips 2.0-2.1 mm, no face penetration, noses 8.3-9.2 mm
    clear, IK misses <= 1 mm, clamp log empty but soft-limit easing (wrist twist 94 -> 92.6°); seated (salt_and_sun
    c3_siesta) lips 9.6 mm, her face 6 mm into his (was 25-26 mm) — the procedural seated kiss is the weak one until
    track G's `kiss.sit` reaches the GLBs.
- **QA:** `kissqa=1` logs `KISS-QA` every frame (lips, deepest face penetration with a 6 mm lateral gate, nose
  clearance, IK miss, pre-IK gap, step); `orbit=yaw:,pitch:,dist:` (story camera on a sphere round the heads);
  `skindbg=1..11`. Renders without a render box: push a `qa/<name>` branch with `tools/qa/run.txt` (list, WxH, frames)
  → `.github/workflows/qa_shots.yml` builds Linux and uploads the PNGs + harness log (artifact `qa-shots-<run>`).

## Found, not fixable in the renderer (hair assets — `tools/model/hair/`)
- Mei's bob: the side panels are opaque card cores side by side (read as one black sheet in shade); a gap above the ear
  shows the scalp cap's polygon edge; a stray card fragment by the jaw (q34 view).
- Vivian's chignon v2 and Daniel's crop: the scalp cap ends in a stair-stepped polygon border above the ear (its
  COLOR_0 fade only runs by height above the hairline, not toward the cap's own border).
- Amara's puff: the coil cards on top cross at random angles (a scrap pile from above); a coily shell under fewer,
  flow-aligned coil cards would read as a puff.
- The strand atlas' dense columns are opaque over 55 % of their texels with strands up to the column border, so cards
  are opaque ribbons; a sparser atlas (strand cores ~30 %, more sheet) would let the OIT pass do more.

## Commands
```bash
# molab (AgentSky task/tools): sync, build, hair views of every style
python3 msync.py /data/src/engine /root/wt_engine && python3 mbuild.py /root/wt_engine /root/build_engine
python3 mshots.py /root/build_engine hair_views.txt /root/qa/<tag> 1920x1080
#   hair_views.txt: screen=customize;tab=her;hermodel=mei;outfit=knit_sweater_jeans;focus=頭;view=yaw:38,pitch:6,dist:0.62;ui=0;t=2
#   add ;hairdbg=1 to see soft layers (green) vs strand cores (red)
```
