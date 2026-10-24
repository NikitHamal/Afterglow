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

## Done
- Engine roster + identity packs + hair styles + Model row in Customize; Mei's face (`mei_identity.bin`, 300 KB).

## In flight / next
- Hair kit (Vivian's chignon rebuilt + one style per new model), skins and eyes for Mei, Amara, Daniel.

## Commands
```bash
# molab (identity): fresh MPFB session, then the base rig
blender -b --python tools/model/identity/identity.py -- mei /root/agb/models/mei_id.npz
blender -b /root/agb/models/vivian_game.blend --python tools/model/identity/identity_apply.py -- mei /root/agb/models/mei_id.npz native/assets/characters/vivian.glb /root/agb/models/mei
# QA stills
./afterglow --shotlist list.txt --outdir out --res 1920x1080   # e.g. screen=customize;tab=her;hermodel=mei;portrait=1;turn=0.6
```
