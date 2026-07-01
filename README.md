# Takeoff Coach 5.9.0 — Live Interception

Takeoff Coach is a BakkesMod Freeplay drill for Fast Touch, Shoot, and Full Random scenarios.

## 5.9.0 behavior

- Fixed 10-second ball prediction horizon.
- `Minimum preparation time before jump` is a real solution constraint, not merely a cue offset.
- Reaction allowance only advances the green cue before the physical ideal jump.
- Initial car velocity is always collinear with the car nose: positive speed drives forward, negative speed drives backward.
- Maximum bounces now selects and validates real bounce tiers. When greater than zero, generated scenarios may deliberately target any bounce count up to the chosen maximum.
- Predicted supported bounces preserve timing. A large velocity change alone no longer marks the path as changed.
- The normal HUD uses a compact em dash for unavailable timing and never prints a path-change paragraph.
- Timing, alignment, and height indicators have a global visibility switch plus individual switches.
- The height gauge spans the complete 0–2044 uu field-height display, so its pointer does not saturate. Target selection remains a physically legal ball-centre range.
- The obsolete `Full height display margin` setting has been removed.

## Height conventions

Target selection uses legal ball-centre heights. The live height gauge is displayed over the full field-height scale from 0 to 2044 uu.

## Build artifact

GitHub Actions produces only:

- `TakeoffCoach.dll`
- `takeoffcoach.cfg`

The obsolete installer command file is not included.

## 5.8 live takeoff correction

The blue takeoff marker is no longer copied from an ideal automatically-steered ground path. The selected ball contact remains stable, while the car continuation and aerial reach are recomputed from the local car's current position, full velocity, nose and current throttle/steer/boost/handbrake input. The same predicted takeoff state is used by the reach test and the marker. Rounded HUD boxes are rendered without overlapping alpha layers.
