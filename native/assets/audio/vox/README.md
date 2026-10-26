# Voice banks

A cue resolves in this order, first hit wins (`Game::voice`):

    vox/<her|him>/<model>/<cue>      e.g. vox/her/amara/moans01.ogg
    vox/<her|him>/<cue>              the role's shared bank
    vox/<cue>                        the legacy flat bank (moan02, takeme, loveyou, suck05, swallow04)

so a model can be as specific as it needs to be and everything it leaves out falls back. Any number of variants per
cue is fine; one is picked at random and never twice in a row.

## Cues the game asks for

| cue | role | when |
|---|---|---|
| `moans` | her | arousal under 0.45: a soft, held-back sound |
| `moan` | her | arousal 0.45+, a desire met, the periodic voice |
| `climax` | her | her orgasm |
| `gasp` | her | a touch landing, a desire beginning |
| `breath-in`, `breath-out` | her | the breathing clock, always running |
| `suck`, `swallow` | her | the oral act's strokes |
| `take`, `love` | her | spoken lines ("take me", said in the afterglow) |
| `groan` | him | his strokes once he is close |
| `climax` | him | his orgasm |

Models: her = `vivian`, `mei`, `amara`; him = `marcus`, `daniel` (`characters/roster.json`). Each model also gets a
fixed pitch identity in `modelPitch()` (Vivian low and warm, Mei bright, Amara full, Marcus lowest, Daniel mid), so
even a shared recording reads as a different person.

## Format

44.1 kHz, mono, `.ogg` (Vorbis ~96 kb/s) or 16-bit `.wav`. Keep the head and tail tight: the engine plays the file
as-is and drives her lip sync from its real length, so silence at the start shows up as a delayed mouth.

## Loudness

Normalise the whole bank to the same loudness (`loudnorm I=-16:TP=-1.5:LRA=11`) — the per-file trim table in
`Game::voice` exists only for the five legacy samples and new files must not need it.

## Licence

Every file here needs a licence that allows use in a game and redistribution in this repo. Record the source and the
exact licence in `LICENSE_sources.md` next to the bank, per folder, and keep any attribution the licence asks for.
