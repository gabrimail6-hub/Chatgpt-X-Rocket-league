# Takeoff Coach 5.3 Runtime Wiring Fix

This build keeps only the two drill objectives: **Fast Touch** and **Shoot**.

Runtime fixes in this build:

- Reaction Cue guidance is copied into every new attempt before target selection.
- The cue is immediately visible in Reaction Cue mode, starts red, turns green once, and never returns to red during the attempt.
- Candidate solutions drive Timing and Angle before target lock.
- Every selected target must be inside the configured ball-centre contact-height band.
- Fast Touch selects the earliest reachable target in that band.
- Shoot selects a reachable, descending, pre-bounce target in that band and aims toward the intended goal.
- Advanced aerial profiles are fixed internal implementation details; they are no longer user-facing settings.
- Advanced solving automatically falls back to the simple solver.
- Boost availability never invalidates a Freeplay target.
- The visible takeoff marker uses live continuation from current velocity, nose, throttle, steering, boost and handbrake instead of displaying the ideal solver's bot-steering path.
- Height uses the real live ball-centre height independently of Timing visibility.
- Setup memory contains only Shoot and Fast Touch setup ranges.

Build identifier shown in Settings: `5.4.0-validation-recovery`.


## Scenario validation and recovery

Shoot scenarios are validated before application: a descending target must exist inside the selected ball-centre height band and before the first collision. Automatic rerolls pause at the rejection cap, while **New Scenario** always clears the cap and starts a fresh generation sequence. The target-height preference is global; Fast Touch and Shoot setup memories contain spawn and motion ranges only.
