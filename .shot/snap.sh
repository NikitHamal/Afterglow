#!/usr/bin/env bash
# usage: .shot/snap.sh <name> <query>   -> writes .shot/<name>.png
set -e
CHROME="/c/Program Files/Google/Chrome/Application/chrome.exe"
NAME="$1"; QUERY="$2"
cd "$(dirname "$0")/.."
ROOT_W="$(pwd -W)"
"$CHROME" --headless=new --disable-gpu --hide-scrollbars \
  --window-size=1280,720 --virtual-time-budget=2600 \
  --screenshot="$ROOT_W/.shot/$NAME.png" \
  "http://127.0.0.1:5500/.shot/shot.html?$QUERY" 2>/dev/null
echo "wrote .shot/$NAME.png"
