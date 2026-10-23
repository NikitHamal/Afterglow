# Story assets — what story mode wants from the content track

Story mode (`native/src/game/story.*`, `game_story.cpp`) plays data-driven visual novels over the live 3D scene.
Scripts live in `native/assets/story/<id>.json` (one file per story; the engine loads every `*.json` there). Until
the assets below land, the engine uses fallbacks: every location renders as the bedroom with the node's lighting
mood and a location card; a character whose outfit is `dressed` or `partial` is **not drawn** (only `nude`
characters appear), so early chapters read as text over the room. Nothing in a script has to change when assets
arrive — the engine picks them up by name.

## Script format (JSON)

```jsonc
{
  "id": "old_flames", "title": "Old Flames", "tagline": "...", "setting": "A COASTAL HOTEL · A WEDDING WEEKEND",
  "mood": "evening",                       // default lighting mood: evening | candle | day
  "vars": {"trust": 0, "warmth": 0},       // numeric variables, reset per playthrough
  "chapters": [{"id": "c1", "title": "The Toast", "start": "c1_reception"}],
  "gallery": {"g_first": "Rediscovery"},   // unlockable scenes (replay from the node that unlocks them)
  "nodes": {
    "c1_reception": {
      "chapter": "c1", "location": "the hotel ballroom", "mood": "evening",
      "show": ["vivian", "marcus"],                       // who is on stage (omit = keep)
      "outfit": {"vivian": "dressed", "marcus": "nude"},  // dressed | partial | nude
      "act": "missionary", "actT": 0.15,                  // pose from a free-play act (clip family) at depth/phase
      "camera": "wide",                                   // wide | side | front | top | face | his_view
      "set": {"warmth": "+1"},                            // on entering: "+n", "-n", "=n"
      "lines": [
        {"who": "narrator", "text": "..."},
        {"who": "vivian", "text": "...", "voice": "moan02", "sfx": "wet", "camera": "face",
         "expr": {"smile": 0.6, "pleasure": 0.3, "moan": 0, "kiss": 0, "flush": 0.2}, "wait": 0}
      ],
      "choices": [{"text": "...", "if": "trust >= 1 || honest >= 1", "set": {"trust": "+1"}, "next": "c2_terrace"}],
      "next": "c2_terrace",                               // when there are no choices
      "play": {"act": "missionary", "goal": "together", "next": "c6_after"},  // hand over to free play
      "end": {"title": "Second Chance", "text": "..."},  // an ending (recorded)
      "gallery": "g_first"                                // unlocks a gallery entry
    }
  }
}
```

Play goals: `together` (the night ends — his climax; `together` var = 1 when shared), `her` (her first climax),
`him`, `time:<seconds>`. After play the story gets `together`, `herClimaxes`, `connection` as variables.
Conditions: `var <op> value|var` with `>= <= == != > <`, joined by `&&` / `||`.

## Locations wanted (one GLB each, same conventions as `room/room.glb`)

| id (script `location`, slugified) | for | notes |
|---|---|---|
| `hotel_ballroom`, `hotel_terrace`, `hotel_corridor` | Old Flames ch. 1–4 | evening; terrace over the sea at night, paper lanterns; corridor with a room door |
| `hotel_room` | Old Flames ch. 5–7 | the bed must match the authoring bed (all clips are authored on it); sea view window |
| `cabin_interior` | The Storm ch. 1–5 | log cabin, stone fireplace (warm flicker light), rug in front of it, couch, snow at the windows |
| `cabin_bedroom` | The Storm ch. 6–7 | authoring bed; wool blankets; candle light |
| `villa_terrace`, `villa_beach` | Salt and Sun ch. 1–5 | bright daylight; white walls, blue shutters, bay below |
| `villa_bedroom` | Salt and Sun ch. 3, 6 | authoring bed; shutters (striped light), white linen |

Suggested file layout: `native/assets/locations/<id>/<id>.glb` + `<id>.json` (room.json schema: `bedTop`, `bed`,
`lights`, `lightmap`, `cameraBounds`, `window`). Rooms without a bed are fine for dressed scenes.

## Outfits wanted (per character, separate meshes skinned to the same rig)

Names the engine will look for: `<character>_outfit_<style>_<layer>` meshes inside the character GLB (or
`characters/outfits/<character>_<style>.glb` skinned to the same skeleton), with an `extras.afterglow.outfit`
block `{style, layer, hidesWhen: ["partial"|"nude"]}`.

| character | styles | layers |
|---|---|---|
| Vivian | `evening_dress` (Old Flames), `knit_sweater_jeans` (The Storm), `sundress_bikini` (Salt and Sun) | `outer` (dress / sweater), `under` (lingerie / bikini) |
| Marcus | `suit_open_collar`, `flannel_jeans`, `linen_shirt_shorts` | `outer`, `under` |

State mapping: `dressed` = all layers, `partial` = `under` only, `nude` = none. Undress transitions are engine
side (a short dissolve per layer + fabric sfx).

## Poses / clips wanted (both GLBs, same naming as today)

Free-play acts are reused for intimate scenes. Story-only clips (loop `kind`, both characters, same world):
`stand.idle` (standing, talking distance), `stand.embrace` (standing kiss), `dance.slow` (slow dance loop),
`sit.bed` (side by side on the bed edge), `undress.her` / `undress.him` (layer-by-layer, 3–4 s),
`lie.cuddle` (after: her head on his chest). With these the engine stops hiding dressed characters.

## Voice lines wanted (`audio/vox/`)

Short, breathy, one take each, 48 kHz WAV like the current ones: `yes_soft`, `dont_stop`, `kiss_me`, `slower`,
`deeper`, `laugh_soft`, `sigh_content`, `gasp_touch`, `his_name` ("Marcus"), and for him `her_name`, `breath_low`.
Story lines reference them by file name (`"voice": "kiss_me"`).
