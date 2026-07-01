# Changelog

## 4.1.0

- Added Fast Touch, Control Touch, Score, and Random Call objectives.
- Added immutable Shooting and Fast Touch built-in presets.
- Added a persistent Custom preset with explicit save/reload controls.
- Preserved the requested Shooting defaults and added broad Fast Touch ranges.
- Kept full away, crossing, and toward ball-direction randomization.
- Fixed Score attempts ending immediately on first contact; goal tracking now continues after touch.
- Fixed an invalid ImGui `ColorEdit3` call.
- Replaced template placeholders with a real C++20/x64 Visual Studio project.
- Made GitHub Actions build from repository-owned sources.
- Added a macOS Heroic/CrossOver installer command.
- Removed unused template, resource, GUI-base, and auxiliary ImGui files.
- No RocketSim dependency.
