# Models notes — track D (hair + new models) on branch `agent-models-hud`

Newest first. Board: molab notebook → tabs models / hair (`/root/board/shots/<tab>/`).

## How a model works (engine)
- `characters/roster.json` lists the models per role (`her` = Vivian's body, `him` = Marcus'). A model shares its role's
  GLB — body, skeleton, every clip, contact, joint range and outfit — and adds only its own files:
  `<id>_identity.bin` (face), `<id>_skin_<tone>.jpg`, `<id>_eye.png`, `<id>_skin_mask.png` (each falls back to the
  role's file) and a hair style `hair/<style>.glb`. So every act and story clip fits each model by construction.
- `scene/identity.*`: the pack's per-vertex position + normal deltas are added to the base GLB's bind data right after
  it loads (before bind height, touch zones and collision are measured), then the vertex buffers are re-uploaded.
- `scene/hairstyle.*`: a hair GLB bound by joint name to the character's skin 0 (like outfits); rigid parts on 頭, loose
  locks on the role's simulated chains `髪.<n>.a..d`; `weight_up/dn` follow the head. The GLB's own hair is skipped
  while a style is worn (`renderer.cpp drawModel`).
- `Character::id` stays the role ("vivian"/"marcus": clips, contacts, outfits, story casting); `Character::asset` is
  the model. `Scene::loadRole(role, model, hair)` reloads one role (only the shown model is in memory).
- `Game::ensureCast(story)` (`game_cast.cpp`): stories always show their own cast (Vivian, Marcus); free play and
  Customize show the profile's pick (`profile.her.model` / `him.model`, saved). Names in the Customize tabs/title and
  the HUD meters come from the roster (`castName`).
- `fixedAge`: a model whose face was built at its own age shows that age and gets no age shapes (the base's `age_*`
  morphs were measured on the base's face; on a younger face they could leave the adult range).
- QA spec keys: `hermodel=<id>`, `himmodel=<id>`, and on Customize `portrait=1` (head camera), `turn=<rad>`,
  `outfit=<style>`, `face=<morph>:<w>,...`.

## How a face is built (`tools/model/identity/`)
- `recipes.json`: the base recipes (copied from build5.py / male/build.py) + each model's head phenotype and face
  targets. Adults only (MakeHuman age >= 0.5 = 25 y, asserted).
- `identity.py` (MPFB, fresh session): MakeHuman(base + model) − MakeHuman(base), masked to head + neck, the skull's
  drift removed (another heritage/age is taller or shorter), eyeballs pinned to the base's eye bones, carried to eyes /
  brows / lashes / teeth / tongue through MakeHuman's MHCLO weights (as build5 does for the body variants).
- `identity_apply.py` (on `<base>_game.blend`): checks the MakeHuman → game vertex correspondence (similarity fit,
  must be < 0.5 mm; measured 0.001 mm), applies the delta to every shape key, writes `<id>_game.blend` (hair styles are
  grown on it) and `<id>_identity.bin` mapped onto the shipped GLB's vertices by rest position (measured 0.0000 mm).

## Done (all rendered on molab with the game renderer; board tabs models / hair)
- Engine roster + identity packs + hair styles + Model row in Customize (`b3919d3`); picking a model gives its own
  skin tone and hair colour (`pickModel`); a story's cast shows its own tone/hair, not the player's pick.
- **Vivian's hair** (`856566a`): chignon rebuilt with the kit and spliced into vivian.glb — feathered hairline,
  combed-back strands (no crown starburst), real tendril locks on her chains, fuller bun. Board: hair/vivian_chignon_v2_before_after.
- **Mei** (`0a34c59`): 31, East Asian heritage, sleek A-line bob (`hair/bob_mei.glb` 7.2 MB), brown eyes, rose lips
  on porcelain / fair / honey. Board: models/mei_portrait, mei_expressions, mei_full, her_roster.
- **Amara** (`7bccf50`): 34, West African heritage, high puff with laid edges (`hair/puff_amara.glb` 6.6 MB), deep
  brown eyes, berry lips on deep / ebony / bronze. Board: models/amara_*.
- **Daniel** (`17ece15`): 34, Mediterranean, textured crop (`hair/crop_daniel.glb` 1.5 MB), green eyes, Marcus'
  skin (clean-shaven). Board: models/daniel_*, his_roster.

## Residuals / next
- Card hair at grazing light (sides of the chignon, top of the bob) still shows card-to-card noise: the engine's card
  shading (alpha test, per-card highlights) — a hair shading pass (alpha-to-coverage or a depth-peeled soft pass,
  per-vertex AO into COLOR_0.rgb) would lift every style at once.
- Hair styles are rigid on 頭 except Vivian's tendrils/nape wisps; Mei's bob could ride Vivian's tendril chains
  (they hang where the bob's front does) once checked for face clearance in every clip.
- vivian.glb still carries the RGBA atlas (3 MB); re-splicing with the palette atlas saves ~2 MB (one more 28 MB commit).
- A model's age is fixed (its face was built at that age); per-model age shapes would need age deltas measured on the
  new face (identity.py could build them the same way build5 does).
- Tones outside a model's painted set fall back to Vivian's / Marcus' albedo (their makeup).

## Commands
```bash
# hair styles (on <model>_game.blend from identity_apply.py, or the base rig); --splice replaces a GLB's own hair
blender -b /root/agb/models/mei/mei_game.blend --python tools/model/hair/build_hair.py -- bob bob_mei <out_dir> --role vivian
blender -b /root/agb/models/vivian_game.blend --python tools/model/hair/build_hair.py -- chignon x <out_dir> --splice <vivian_orig.glb> <out.glb>
# eyes and skins (local, from the shipped files)
python3 tools/model/native/eye_tex.py native/assets/characters/vivian.glb native/assets/characters/mei_eye.png --iris 0.26,0.15,0.085 --tint 0.88 --gain 0.9
python3 tools/model/identity/skin_variant.py mei native/assets/characters/vivian.glb native/assets/characters
# molab (identity): fresh MPFB session, then the base rig
blender -b --python tools/model/identity/identity.py -- mei /root/agb/models/mei_id.npz
blender -b /root/agb/models/vivian_game.blend --python tools/model/identity/identity_apply.py -- mei /root/agb/models/mei_id.npz native/assets/characters/vivian.glb /root/agb/models/mei
# QA stills
./afterglow --shotlist list.txt --outdir out --res 1920x1080   # e.g. screen=customize;tab=her;hermodel=mei;portrait=1;turn=0.6
```
