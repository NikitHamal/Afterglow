# Engine notes — track H (round 3: hair passes, skin bands, the kiss at runtime, side/profile) on `agent-models-hud`

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
