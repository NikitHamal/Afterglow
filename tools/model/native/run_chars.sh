#!/bin/bash
# Rebuild both characters for the native game, one after the other (2-core / 8 GB friendly).
#   bash tools/model/native/run_chars.sh [female|male|both]   (from the repo root)
D=$(cd "$(dirname "$0")" && pwd)
case "${1:-both}" in
  female) bash $D/run_female.sh ;;
  male)   bash $D/run_male.sh ;;
  wait-male)
    while ! grep -q FEMALE_PIPELINE_DONE /root/agb/female_run.log 2>/dev/null; do sleep 20; done
    bash $D/run_male.sh ;;
  *)      bash $D/run_female.sh; bash $D/run_male.sh ;;
esac
