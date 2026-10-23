#!/bin/sh
# Adds Afterglow to your application menu (per user). Run it from the unpacked Afterglow folder.
# Remove with: rm ~/.local/share/applications/afterglow.desktop ~/.local/share/icons/hicolor/256x256/apps/afterglow.png
set -e
here="$(cd "$(dirname "$0")" && pwd)"
data="${XDG_DATA_HOME:-$HOME/.local/share}"
mkdir -p "$data/applications" "$data/icons/hicolor/256x256/apps"
cp "$here/afterglow.png" "$data/icons/hicolor/256x256/apps/afterglow.png"
sed "s|^Exec=.*|Exec=\"$here/afterglow\"|; s|^Icon=.*|Icon=afterglow|" "$here/afterglow.desktop" > "$data/applications/afterglow.desktop"
printf 'Path=%s\n' "$here" >> "$data/applications/afterglow.desktop"
chmod +x "$data/applications/afterglow.desktop"
echo "Afterglow added to your applications menu."
