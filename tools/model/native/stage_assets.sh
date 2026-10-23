#!/bin/bash
# Copy the built native assets into native/assets (characters, tones, audio) and regenerate clips.json.
#   bash tools/model/native/stage_assets.sh            (from the repo root)
set -e
R=$(cd "$(dirname "$0")/../../.." && pwd)
A=$R/native/assets
mkdir -p $A/characters $A/audio/sfx $A/audio/vox
cp /root/agb/native/export/vivian.glb /root/agb/native/export/marcus.glb $A/characters/
cp /root/agb/native/tex/vivian/vivian_skin_{honey,caramel,deep}.jpg /root/agb/native/tex/marcus/marcus_skin_{honey,caramel,deep}.jpg /root/agb/native/tex/marcus/marcus_genital_*.jpg $A/characters/
# optional skin support maps (tools/model/native/skin_maps.py): flush / translucency / sweat masks + tileable micro normal
for f in /root/agb/native/tex/vivian/vivian_skin_mask.png /root/agb/native/tex/marcus/marcus_skin_mask.png /root/agb/native/tex/skin_detail_n.png; do
  [ -f "$f" ] && cp "$f" $A/characters/
done
SP="$R/assets/Shinlalala's Lewd Sound Pack"
for d in Cum DryPlaps PullOut Strokes Wet WetPlaps; do
  dst=$A/audio/sfx/$(echo $d | tr 'A-Z' 'a-z'); mkdir -p "$dst"; cp "$SP/$d/"*.ogg "$dst/"
done
cp "$SP/README.txt" $A/audio/sfx/LICENSE_soundpack.txt
cp $R/assets/vox/*.wav $A/audio/vox/
/root/agb/blender-4.2.3-linux-x64/blender -b /root/agb/native/anim.blend --python $R/tools/model/anim/clips_json.py -- $A/characters/clips.json 2>&1 | grep -E "clips.json|Traceback|Error" || true
if [ -f /root/agb/native/room/room.glb ]; then mkdir -p $A/room; cp /root/agb/native/room/room.glb /root/agb/native/room/room.json $A/room/; fi
du -sh $A/* ; ls $A/characters
