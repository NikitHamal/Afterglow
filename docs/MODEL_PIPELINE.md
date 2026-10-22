# Realistic character pipeline (Vivian, Marcus)

Both realistic characters are generated from scripts — no hand-sculpting — so any session can
rebuild or tweak them. Heavy work runs on a remote Linux box (the "molab" marimo notebook); any
Linux machine with ~16 GB RAM works.

## Toolchain

- Blender 4.2 LTS (`blender-4.2.3-linux-x64`) + the **MPFB 2** extension (MakeHuman for Blender),
  installed with `blender -b --command extension install-file -r user_default -e mpfb.zip`.
- MakeHuman **CC0** system assets (`makehuman_system_assets_cc0.zip`) symlinked into MPFB's user
  data dir (`~/.config/blender/4.2/extensions/.user/user_default/mpfb/data/`).
- Pillow inside Blender's Python (`<blender>/4.2/python/bin/python3.11 -m pip install pillow`).
- Node + `@gltf-transform/core|extensions|functions@4` for GLB post-processing.

## Female (`tools/model/female/`)

| Step | Script | Output |
|---|---|---|
| Body, face, expressions, 4K skin in 3 tones | `build4.py` | `out/vivian_s4.blend`, `out/tex/vivian_skin_{honey,caramel,deep}.jpg`, normal, ORM, eye, brow |
| Procedural hair cards + scalp paint | `hair.py` | `out/vivian_s5.blend`, `out/tex/vivian_hair.png` |
| MMD skeleton (Goat-chan frames), weights, scale 1.35 m, GLB | `rig_export.py` | `out/export/vivian_raw.glb`, `out/vivian_game.blend`, preview GLB |
| Compact textures | `textures_small.py` | 2K normal/ORM, game skin JPEGs |
| Game GLB | `node optimize.mjs raw.glb vivian.glb out/tex` | `vivian.glb` (~11 MB) |
| Hand-target offsets for `GLB_REGISTRY.vivian.land` | `landmarks_game.py` | printed JSON |
| Review renders | `render_tones.py`, `render_hair.py`, `render_expr.py` | PNG sheets |

Key ideas:
- Phenotype: gender 0, age 0.63 (~42), mixed heritage, curvy hourglass + ~30 sculpt targets.
- Expression morphs are mixes of MakeHuman expression units, transferred to lashes/brows/teeth/
  tongue through each part's MHCLO barycentric binding: `blink, moan, pleasure, oral, kiss, smile`.
- Skin is painted procedurally in UV space from 4K bakes (position, normal, masks, AO): makeup,
  geometric lip mask (mouth line from the mouth-open morph), brow underlay transferred from the
  brow cards, areolae, nails, freckles; tones are calibrated in linear light from the MakeHuman
  light/dark textures.
- Hair: Poisson-disk roots on the real scalp, side part, gravity + body-collision growth, clumping,
  helical waves, two card layers, generated strand atlas (neutral grey, tinted by `hairColor`).
- Skeleton: MPFB `game_engine_with_breast` weights renamed to MMD bones; each bone's roll copies the
  Goat-chan bone's X axis (game driver depends on it); finger curl axes come from the nail direction.

## Male (`tools/model/male/`)

`build.py` → `genitals.py` → `paint.py` → `hair.py` → `rig_export.py` → `textures_small.py` →
`optimize.mjs`; review with `render.py`/`check.py`. 68 bones (MMD set + `竿根/竿中/竿先`), morphs
`blink, effort, pleasure, moan, kiss, body_muscle, body_weight, body_belly, penis_length, penis_girth`.
Game scale 1.70 (matches the procedural male). Integration: `js3d/glbHim.js`.

## Remote helpers (`tools/model/`)

`mx` (run Python in the molab scratchpad), `msh` (shell there), `mbg` (detached job), `mup`
(upload to `/root/agb/`), `mfetch` (md5-checked chunked download), `ghpush.py` (push the working
tree through the GitHub connector). They need the molab URL + token the session was given.
