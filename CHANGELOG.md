# Takeoff Coach 5.8.0 — Live Takeoff

- Blue takeoff marker and reachability simulation now use the same live grounded continuation from the current car position, velocity, nose and controller input.
- The locked contact target remains stable, while the projected takeoff position keeps updating until the real jump.
- Removed ideal automatic ground steering from the published aerial solution.
- Rounded HUD rectangles are now filled once per scanline, eliminating the darker double-opacity rectangle underneath.

# 5.8.0 — Reach + Bounce Fix

- Fixed prediction horizon at 10 seconds.
- Replaced cue-lead semantics with a true minimum preparation-time constraint, adjustable from 0 to 6000 ms and defaulting to 1500 ms.
- Kept reaction allowance separate and capped at 1000 ms.
- Made scenario spawn velocity exactly forward or backward along the car nose.
- Added deliberate bounce-tier generation up to `Maximum bounces` and strict runtime selection of the generated tier.
- Reworked generation validation to simulate supported bounces instead of rejecting every first collision.
- Added directional inertia checks using forward speed, lateral speed, and heading correction time.
- Prevented ordinary velocity changes from falsely invalidating timing as a path change.
- Replaced verbose path-change feedback with compact unavailable timing.
- Added a global indicator visibility toggle.
- Removed `Full height display margin`; height gauge now maps the full 0–2044 uu field-height range.
- Updated version strings and configuration defaults.
