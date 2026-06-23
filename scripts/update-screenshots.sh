#!/usr/bin/env bash
#
# Regenerate the README / GitHub Pages screenshots in docs/assets/screenshots/.
#
# The app has a headless capture mode (--screenshots) that runs a scripted demo
# workout and dumps PNGs, then quits. This script runs that mode in dark theme
# (what the docs use) and copies the freshly captured images over the ones the
# docs already reference. Files the docs use but the capture mode does not
# produce (e.g. screenshot_retro_race.png) are left untouched.
#
# Usage:
#   scripts/update-screenshots.sh [path-to-MaximumTrainer-binary]
#
# Defaults to build/release/MaximumTrainer. Build first (see CLAUDE.md) if it
# does not exist. QWT in a non-standard prefix needs LD_LIBRARY_PATH set, e.g.:
#   LD_LIBRARY_PATH=/tmp/qwt6/lib scripts/update-screenshots.sh
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${1:-$REPO_ROOT/build/release/MaximumTrainer}"
DEST="$REPO_ROOT/docs/assets/screenshots"
SHOTS="$(mktemp -d)"
CONF="$HOME/.config/MaximumTrainer/MaximumTrainer_Redux.conf"

# Capture-mode output name -> docs filename. Most match 1:1; the Bluetooth page
# is captured as "sensors" but the docs call it "devices".
declare -A RENAME=( [screenshot_sensors.png]=screenshot_devices.png )

cleanup() {
  # Restore the user's real settings if we forced the theme.
  [[ -f "$SHOTS/conf.bak" ]] && cp -f "$SHOTS/conf.bak" "$CONF"
  rm -rf "$SHOTS"
}
trap cleanup EXIT

if [[ ! -x "$BIN" ]]; then
  echo "error: binary not found at $BIN" >&2
  echo "       build it first (see CLAUDE.md) or pass the path as an argument." >&2
  exit 1
fi

# Force dark theme for the capture (the docs use dark mode). Back up the whole
# settings file and restore it verbatim afterwards, so nothing the app rewrites
# during the throwaway capture run leaks into the user's real settings.
if [[ -f "$CONF" ]]; then
  cp "$CONF" "$SHOTS/conf.bak"
  if grep -q '^app_theme=' "$CONF"; then
    sed -i 's/^app_theme=.*/app_theme=1/' "$CONF"
  else
    sed -i '0,/^\[General\]/s//[General]\napp_theme=1/' "$CONF"
  fi
else
  echo "note: no settings file at $CONF; capturing with the default theme." >&2
fi

echo "Capturing screenshots to $SHOTS ..."
"$BIN" --screenshots "$SHOTS"

echo "Updating docs screenshots in $DEST ..."
updated=0
for src in "$SHOTS"/*.png; do
  [[ -e "$src" ]] || continue
  name="$(basename "$src")"
  target="${RENAME[$name]:-$name}"
  # Only refresh images the docs already use; don't dump unused captures.
  if [[ -f "$DEST/$target" ]]; then
    cp "$src" "$DEST/$target"
    echo "  updated $target"
    updated=$((updated + 1))
  fi
done

echo "Done. $updated image(s) updated."
echo "Review with 'git diff --stat docs/assets/screenshots' and commit what you want."
