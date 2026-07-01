# Changelog

## 5.3.0-runtime-wiring

- Fixed missing `tc_guidance_style` assignment when a scenario starts.
- Reset cue and reaction state on every scenario.
- Made the candidate solution feed Timing and Angle before lock.
- Enforced the full target-height range for every selection path.
- Removed Reaction target-policy controls and random-policy behavior.
- Replaced wall-clock-aborted candidate solving with a deterministic 12-finalist pass.
- Replaced weighted target ranking with explicit Fast Touch and Shoot ordering.
- Added strict pre-bounce selection with one global optional post-bounce fallback.
- Prevented Shoot generation from accepting a ceiling/wall/floor collision before a descending target crossing.
- Removed boost-deficit target rejection and user-facing profile/backend controls.
- Replaced displayed bot-steering takeoff positions with live-input continuation estimates.
- Removed Custom preset storage and non-setup fields from preset memory.
- Made Height independent from Timing visibility.
- Added visible build identifier and cue color controls.
