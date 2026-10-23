#!/bin/bash
# Rebuild Vivian at real scale for the native game (1.68 m body height).
#   bash tools/model/native/run_female.sh            (from the repo root)
# Stages: build5 (body, morphs, vulva, 4K skin) -> hair -> rig_export (MMD rig, real scale) -> native export.
set -u
R=$(cd "$(dirname "$0")/../../.." && pwd)
F=$R/tools/model/female; N=$R/tools/model/native
B=/root/agb/blender-4.2.3-linux-x64/blender
mkdir -p /root/agb/out/tex /root/agb/out/export /root/agb/out/preview
cp -f $F/progress.py /root/agb/progress.py
cd /root/agb
step() { echo "=== $1 $(date +%T)"; shift; "$@"; local rc=$?; echo "=== rc=$rc $(date +%T)"; }
[ -f /root/agb/goat_bones.json ] || step goat $B -b --python $N/dump_goat.py
step build5 $B -b --python $F/build5.py
grep -q "stage4\|vivian_s4" /root/agb/out/build5.done 2>/dev/null
step hair $B -b /root/agb/out/vivian_s4.blend --python $F/hair.py
AG_HEIGHT=1.68 AG_BODY_ONLY=1 step rig $B -b /root/agb/out/vivian_s5.blend --python $F/rig_export.py
echo FEMALE_PIPELINE_DONE
