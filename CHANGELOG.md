# Changelog

## 5.4.0 — Validation and recovery

- Manual **New Scenario** now clears the automatic-reroll latch and invalidates stale callbacks.
- Automatic rerolls stop recoverably and instruct the user to press New Scenario.
- Scenario generation now validates a pre-collision target before applying the setup.
- Shoot vertical velocity is resampled against available ceiling headroom.
- Shoot accepts only descending, in-band, pre-collision targets by default.
- Generated target and initial reach estimate seed runtime verification instead of being discarded.
- Target contact height is global and no longer stored in Fast/Shoot setup memory.
- Ball-centre target-height UI is explicitly 93–1951 uu.
- Solver/Advanced and Diagnostics are hidden from the production settings UI.
- The working reaction cue and Fast/Shoot setup memories are preserved.
