#!/bin/bash
# Rebuild Marcus at real scale for the native game (1.83 m body height).
#   bash tools/model/native/run_male.sh               (from the repo root)
# Stages: build (body, variants, expressions) -> genitals (soft + aroused) -> paint (skin) -> hair -> rig_export.
set -u
R=$(cd "$(dirname "$0")/../../.." && pwd)
M=$R/tools/model/male; N=$R/tools/model/native
B=/root/agb/blender-4.2.3-linux-x64/blender
mkdir -p /root/agb/male/out /root/agb/male/tex /root/agb/male/export /root/agb/out/preview
[ -f /root/agb/progress.py ] || cp -f $R/tools/model/female/progress.py /root/agb/progress.py
cd /root/agb/male
step() { echo "=== $1 $(date +%T)"; shift; "$@"; local rc=$?; echo "=== rc=$rc $(date +%T)"; }
[ -f /root/agb/goat_bones.json ] || step goat $B -b --python $N/dump_goat.py
step build $B -b --python $M/build.py
step genitals $B -b out/marcus_a.blend --python $M/genitals.py
step paint $B -b out/marcus_b.blend --python $M/paint.py
step hair $B -b out/marcus_c.blend --python $M/hair.py
AG_HEIGHT=1.83 AG_BODY_ONLY=1 step rig $B -b out/marcus_d.blend --python $M/rig_export.py
echo MALE_PIPELINE_DONE
