# Changelog

## 5.2.0 Functional Baseline

- Made Timing, Angle, and Height use explicit live/frozen sources.
- Replaced the cue effects/audio path with a single reliable red/green rectangle.
- Added a bounded coarse-to-fine candidate/profile search and automatic simple fallback.
- Added real controller input capture for grounded continuation inputs.
- Wired horizontal and vertical calibration into reach simulation.
- Replaced hardcoded boost availability with the local car boost component.
- Connected Shoot aim/speed scoring switches, setup rejection limit, and marker-before-cue master control.
- Added solve budgets, backend selection, takeoff confidence threshold, and runtime diagnostics.
- Kept Custom preset storage connected through the generic preset schema.
- Added `cvar_audit.txt` and repository metadata/build-output exclusions.
- Removed cue sound, audio settings, and external sound-file dependencies.

## Known limitations

- The advanced aerial controller is still a bounded reachability approximation, not a full Rocket League car dynamics replica.
- Shoot output is explicitly an estimated impulse/corridor score, not a guaranteed physical shot.
- Expensive search is bounded on the game thread; it has not yet been moved to a worker thread.
- Ball collision geometry remains approximate in curved and overlapping goal regions.
