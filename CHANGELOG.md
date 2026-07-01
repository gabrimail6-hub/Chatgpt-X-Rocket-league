# 5.6.0 — Safety + Full Random

- Added Full Random mode and setup memory with the full wide ranges requested.
- Added signed car speed down to -1800 uu/s.
- Removed Car velocity angle from code, config, presets, and UI.
- Shoot vertical-speed minimum is now 0 uu/s.
- Increased reaction allowance limit to 1000 ms.
- Added a 0–3000 ms minimum cue-lead safety setting, default 750 ms; Reaction allowance is added to the accepted jump delay.
- Added rounded HUD panel and cue rectangles.
- Kept maximum-bounce and auto-load matching setup behavior.

# Changelog

## 5.5.0
- Advanced reaction cue by the human allowance while retaining physical timing zero.
- Reworked Fast Touch and Shoot generation around coherent interception routes.
- Added working maximum-bounces selection limit, default 0.
- Restored optional automatic setup loading on objective change, enabled by default.
- Height freezes only on a valid touch; misses do not create a frozen height result.
- Indicator text scale now has a hard 1.00 minimum.
- Added rounded ImGui controls.
- Replaced verbose path-change timing failure with a compact state.
- Removed the obsolete installer command from source and build artifacts.
