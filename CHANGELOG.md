# 5.7.0 — Reach + Bounce Fix

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
