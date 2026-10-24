# Story assets — what story mode uses, and what it still wants from the content track

Story mode plays each story as a short film over the live 3D scene (`native/src/game/game_cinema.cpp`, `story.*`,
`director.*`, `voice.*`, `scene/actor.*`, `scene/outfit.*`). Scripts live in `native/assets/story/<id>.json`, written
with the helpers in `tools/story/` (`python3 tools/story/<id>.py native/assets/story` re-emits and validates one).
Every spoken line is voiced by `.github/workflows/voices.yml` (see `docs/VOICE.md`) when a script changes.

The engine always stages a scene with what exists today and picks up new assets **by name**, without script
changes: a missing location falls back to the closest one that exists (hotel_* → `hotel_room`, cabin_* →
`cabin_bedroom`, villa_* / beach → `villa_bedroom`, spa_* → `hotel_room`, apartment_* / city → the default room); a
missing staging clip falls back along a chain (`stand.embrace` → `stand.idle`, `cuddle.idle` → spooning
still, `dance.slow` → `stand.idle`) with procedural acting on top.

## Script format (JSON)

```jsonc
{
  "id": "old_flames", "title": "Old Flames", "tagline": "...", "setting": "A COASTAL HOTEL · A WEDDING WEEKEND",
  "mood": "evening",                                        // default lighting mood: evening | candle | day
  "cast": {"vivian": {"wardrobe": "evening_dress"}, "marcus": {"wardrobe": "suit_open_collar"}},
  "vars": {"trust": 0}, "chapters": [{"id": "c1", "title": "The Toast", "start": "c1_reception"}],
  "gallery": {"g_first": "Rediscovery"},
  "nodes": {
    "c1_reception": {
      "chapter": "c1", "location": "the ballroom of the Hotel Miramar",   // shown on the location card
      "place": "hotel_ballroom",                         // location asset wanted (falls back, see above)
      "mood": "evening", "ambience": "party",            // party | sea | rain | wind | fire | city | spa, '+'-combined
      "show": ["vivian", "marcus"], "outfit": {"vivian": "dressed", "marcus": "dressed"},   // dressed | partial | nude
      "wardrobe": {"vivian": "knit_sweater_jeans"},      // per-node style change (optional)
      "pose": "stand",                                   // stand | sit | lie | dance | embrace | a clip name | an act id
      "act": "missionary", "actT": 0.1,                  // the act a to_bed action goes to (and its depth)
      "lines": [
        {"who": "marcus", "text": "Vivian.",
         "delivery": "soft",                             // TTS + acting energy: normal soft whisper playful urgent breathless tender
         "shot": "close",                                // establish wide two ots close xclose kiss medium (act poses: side front top face pov)
         "gesture": "open", "look": "down", "head": "nod",   // acting cues for the speaker
         "react": "smile",                               // the listener's reaction
         "expr": {"smile": 0.4, "her.shy": 0.3},         // face morphs (speaker's, or her./him.)
         "action": "kiss",                               // kiss embrace step_in part apart undress:<who>[:outer|under|all]
                                                         // dress:<who> to_bed sit stand lie
         "vox": "moan02", "sfx": "wet", "voice": "none"} // samples on the line; voice none = silent
      ],
      "choices": [{"text": "...", "if": "trust >= 1 || honest >= 1", "set": {"trust": "+1"}, "next": "c2_terrace"}],
      "next": "c2_terrace",
      "play": {"act": "missionary", "goal": "together", "next": "c6_after", "acts": ["missionary", "cowgirl"],
               "beats": {"start": "c5_b_start", "desire": "c5_b_desire", "edge": "c5_b_edge", "her_peak": "c5_b_peak"}},
      "end": {"title": "Second Chance", "text": "..."}, "gallery": "g_first"
    },
    "c5_b_start": {"beat": true, "lines": [{"who": "vivian", "text": "Slowly.", "delivery": "breathless"}]}
  }
}
```

Shots without a `shot` are chosen by the director (narration: establishing then two-shots; dialogue: over-the-
shoulder / close-ups by emotion, held while one speaker goes on, cut when the speaker changes; a kiss gets the
kiss two-shot). Beat nodes are spoken over the play (start, a new desire, his edge, her peak).

## Stories (5 × 7 chapters, 3 endings each)

| Story | Places | Wardrobe (her / him) | Ambience |
|---|---|---|---|
| Old Flames | hotel_ballroom, hotel_terrace, hotel_corridor, hotel_room | evening_dress / suit_open_collar | party, sea |
| The Storm | cabin_door, cabin_living, cabin_bedroom | knit_sweater_jeans / flannel_jeans | wind + fire |
| Salt and Sun | villa_terrace, villa_beach, villa_bedroom | sundress_bikini / linen_shirt_shorts | sea |
| The Massage | spa_lobby, spa_steam, spa_suite, spa_lounge | sundress_bikini / linen_shirt_shorts (spa: `partial`) | spa |
| Neighbours | apartment_hall, apartment_living, apartment_balcony, apartment_bedroom | knit_sweater_jeans / suit_open_collar | city |

## Delivered (thank you) and wanted

Legend: ✅ in the game · ⏳ wanted (the engine falls back meanwhile)

**Locations** (`locations/<id>/<id>.glb` + `.json`, room.json schema; rooms without a bed are fine — standing
scenes use the floor at y = 0 and the foot-of-bed marks of `stand.idle`; places without a bed are built by
`tools/model/room/build_place.py`, whose header spells out the staging contract; a missing place falls back to a
built sibling of its family first, e.g. `spa_*` → `spa_suite`, then the family's bedroom):

- ✅ `hotel_room`, `cabin_bedroom`, `villa_bedroom`
- ✅ `cabin_living` (`tools/model/room/build_place.py`): fieldstone hearth with a flickering fire light (`fire_0`), an
  inglenook dais under an oatmeal sofa at the 0.604 m seat (sit.bed works on it), iron candle chandelier over the
  rug where they stand, reading corner, snow at three windows. `cabin_door` scenes fall back to it.
- ⏳ `hotel_ballroom` (warm chandeliers, a bar, a dance floor), `hotel_terrace` (stone balustrade over the sea at night,
  paper lanterns), `hotel_corridor` (carpet, numbered doors, wall sconces)
- ⏳ `cabin_door` (the porch and door from inside)
- ⏳ `villa_terrace` (white walls, blue shutters, table for two, the bay below), `villa_beach` (a sand crescent,
  rocks, moonlight variant)
- ✅ `spa_suite` (`build_place.py`): a padded treatment bed exactly on the authoring bed's footprint (sheet top 0.604 m:
  every bed clip and sit.bed work on it), limestone and teak, a glowing shoji wall, a paper lantern key light,
  candles on a bench; `spa_lobby` / `spa_steam` / `spa_lounge` fall back to it until they exist.
- ⏳ `spa_lobby`, `spa_steam` (tiled steam room, benches), `spa_lounge` (loungers, tea)
- ✅ `apartment_hall` (`places_apartment.py`): the fourth-floor landing — her door 4A and his 4B facing each other,
  both really open (nodes `door_4a` / `door_4b`, see "Doors and anchors" below), a lit entry behind each, her moving
  boxes by 4A, bells, doormats, sconces, mailboxes, the stairs down under a window on the night city.
- ⏳ `apartment_living` (sofa, piano, city window),
  `apartment_balcony`, `apartment_bedroom` (city view)

**Outfits** (`characters/outfits/<who>_<style>.glb`):

- ✅ Vivian `evening_dress`, `knit_sweater_jeans`, `sundress_bikini`; Marcus `suit_open_collar`, `flannel_jeans`,
  `linen_shirt_shorts` — bound by joint name, every body morph followed, undress layers animated in the engine.
- ⏳ smoother garment borders: the necklines / hems follow the body's triangle edges and read jagged in close-ups
  (a few mm of edge loop or an alpha-faded border strip would fix it).
- ⏳ spa robes (both), a slip / nightdress (her), a white shirt worn open (him).

**Story clips** (both characters, same world, loops unless noted):

- ✅ `stand.idle`, `sit.bed`
- ⏳ `stand.embrace` (standing, his hands at her waist, hers on his chest), `kiss.stand` (standing kiss — today the
  engine steps them in and bends heads procedurally), `dance.slow`, `cuddle.idle` (authored, held back),
  `kiss.side` (authored, held back), `undress.her` / `undress.him` (3–4 s, layer by layer), `sit.couch` / `sit.floor`
  (by the fire), `walk.in` (a few steps into frame), `lean.rail` (both at a balustrade)
- ⏳ talk gestures / listening loops (the engine layers procedural gestures today)
- ⏳ `massage.back` (one lying face down on the bed, the other kneeling beside with hands on the back)

**Voices**: ✅ Kokoro takes for every line (`docs/VOICE.md`). ⏳ extra vocal samples for play: `gasp_touch`,
`laugh_soft`, `sigh_content` (48 kHz WAV like `audio/vox/*`).


## Doors and anchors (places built by `build_place.py` publish them in `<id>.json`)

- `"anchors": {"<name>": {"pos": [x, y, z], "yaw": deg}}` — engine coordinates (metres, y up), yaw about +Y with
  0 = facing +Z. Standing / seating marks: pos on the floor or seat, yaw = the way the person faces. Prop spots: pos on
  the surface (`table`, `counter`, `boxes`, `side_table` ...). Door marks: `door_<id>_out` (on the hall side, facing
  the door), `door_<id>_in` (inside, facing out), `bell_<id>` (the bell button).
- `"doors": [{"id": "4b", "node": "door_4b", "openDeg": 100, "hinge": [...], "width": 0.9}]` — the GLB node has an
  identity rest rotation and its origin on the hinge line (floor level): rotating it about engine +Y by up to
  `openDeg` (signed) swings it open into the apartment. The leaf is baked closed.
