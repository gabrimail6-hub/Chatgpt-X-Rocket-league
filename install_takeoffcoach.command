#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")"
DLL="$PWD/TakeoffCoach.dll"
CFG="$PWD/takeoffcoach.cfg"

if [[ ! -f "$DLL" ]]; then
  echo "TakeoffCoach.dll is missing from this folder."
  echo "Download the compiled TakeoffCoach GitHub Actions artifact, then keep the DLL beside this installer."
  read -r -p "Press Return to close..."
  exit 1
fi

search_roots=(
  "$HOME/Library/Application Support/heroic"
  "$HOME/Games/Heroic"
  "$HOME/Library/Application Support/CrossOver/Bottles"
)

plugin_dirs=()
while IFS= read -r -d '' dir; do
  plugin_dirs+=("$dir")
done < <(
  for root in "${search_roots[@]}"; do
    [[ -d "$root" ]] || continue
    find "$root" -type d -path '*/drive_c/users/*/AppData/Roaming/bakkesmod/bakkesmod/plugins' -print0 2>/dev/null || true
  done
)

if [[ ${#plugin_dirs[@]} -eq 0 ]]; then
  echo "No BakkesMod plugins folder was found automatically."
  echo "Drag your .../bakkesmod/bakkesmod/plugins folder into this Terminal window, then press Return:"
  read -r manual
  manual="${manual%\'}"; manual="${manual#\'}"
  manual="${manual%\"}"; manual="${manual#\"}"
  [[ -d "$manual" ]] || { echo "That folder does not exist."; exit 1; }
  plugin_dirs=("$manual")
fi

if [[ ${#plugin_dirs[@]} -gt 1 ]]; then
  echo "Choose the Heroic/Wine installation to update:"
  select selected in "${plugin_dirs[@]}"; do
    [[ -n "${selected:-}" ]] && { plugin_dir="$selected"; break; }
  done
else
  plugin_dir="${plugin_dirs[0]}"
fi

cfg_dir="$(dirname "$plugin_dir")/cfg"
mkdir -p "$plugin_dir" "$cfg_dir"
cp -f "$DLL" "$plugin_dir/TakeoffCoach.dll"
if [[ -f "$CFG" ]]; then
  if [[ -f "$cfg_dir/takeoffcoach.cfg" ]]; then
    cp -f "$CFG" "$cfg_dir/takeoffcoach.default.cfg"
    echo "Existing user config preserved; new defaults saved as takeoffcoach.default.cfg."
  else
    cp -f "$CFG" "$cfg_dir/takeoffcoach.cfg"
  fi
else
  echo "takeoffcoach.cfg is missing from this folder."
  read -r -p "Press Return to close..."
  exit 1
fi

plugins_cfg="$cfg_dir/plugins.cfg"
touch "$plugins_cfg"
if ! grep -Eiq '^[[:space:]]*plugin[[:space:]]+load[[:space:]]+TakeoffCoach([[:space:]]|$)' "$plugins_cfg"; then
  printf '\nplugin load TakeoffCoach\n' >> "$plugins_cfg"
fi

config_cfg="$cfg_dir/config.cfg"
touch "$config_cfg"
if ! grep -Eiq '^[[:space:]]*exec[[:space:]]+takeoffcoach([[:space:]]|$)' "$config_cfg"; then
  printf '\nexec takeoffcoach\n' >> "$config_cfg"
fi

echo
echo "Takeoff Coach installed successfully."
echo "Plugin: $plugin_dir/TakeoffCoach.dll"
echo "Config: $cfg_dir/takeoffcoach.cfg"
echo "Restart Rocket League/BakkesMod, then open F2 > Plugins > Takeoff Coach."
read -r -p "Press Return to close..."
