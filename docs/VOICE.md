# Story voice acting

Every spoken line in `native/assets/story/*.json` is voiced by **Kokoro-82M** (Apache-2.0, open weights),
synthesised on GitHub Actions by `.github/workflows/voices.yml` → `tools/voice/gen_voices.py`, and committed to
`native/assets/story/voice/` (mono OGG Vorbis, 24 kHz, ~-20 dBFS RMS, trimmed). Edit a story, push, and the
workflow voices only the lines that changed (files are content-addressed), deletes orphans, commits, and
re-runs the `native` build so the nightly carries the new voices.

## Cast

| Speaker | Kokoro voice mix | Speed |
|---|---|---|
| Vivian | af_heart 0.62 + af_bella 0.38 | 0.95 |
| Marcus | am_michael 0.58 + am_fenrir 0.42 | 0.94 |
| Narrator | bf_emma 0.72 + bf_isabella 0.28 (British pipeline) | 0.93 |

## Delivery (optional `"delivery"` on a story line)

`normal`, `soft`, `whisper` (blends in af_nicole / am_onyx, slower, high-passed), `playful`, `urgent`,
`breathless`, `tender`. When a line has none: a preceding narrator line that says whisper/murmur/"against his
ear" → `whisper`; softly/quietly/gently → `soft`; a character line in an intimate node (node has `act`, or an
outfit is `partial`/`nude`) → `tender`; otherwise `normal`. `"voice": "none"` keeps a line silent.

## Manifest (`voice/manifest.json`) — what the engine reads

```json
{ "version": 1, "rate": 24000, "envFps": 30,
  "lines": { "<storyId>": { "<nodeId>": ["<key>", null, "<key>", ...] } },   // aligned with node.lines
  "files": { "<key>": { "who": "vivian", "delivery": "soft", "dur": 2.25,
                        "env": "<base64 uint8 loudness envelope at envFps, 0..255>",
                        "words": [[0.12, 0.41], ...],    // word start/end seconds (lip-sync / subtitle karaoke)
                        "text": "first 120 chars" } } }
```

Playback: `voice/<key>.ogg`. Lip sync: drive jaw/mouth-open from `env` (sampled at the playback time,
smoothed ~40 ms, gated by `words` so the mouth closes between words), plus small viseme variety. Auto-advance can
wait for `dur` + a beat. A missing manifest entry means the line is silent (fallback: typewriter only).

## Regenerate locally

```
pip install torch --index-url https://download.pytorch.org/whl/cpu
pip install "kokoro>=0.9.4" soundfile numpy "misaki[en]" <en_core_web_sm wheel>   # + apt: espeak-ng ffmpeg
python tools/voice/gen_voices.py [--only old_flames] [--limit 20]
```
