# Takeoff Coach 5.2 — Functional Baseline

Takeoff Coach is a BakkesMod freeplay drill for **Fast Touch** and **Shoot**.

## Core runtime outputs

- **Timing** is displayed only from a locked reachable target. While grounded it is `current time - ideal jump time`; it freezes on the first real jump edge.
- **Angle** is the signed 2D angle between the car's actual nose and the vector from the live car position to the stored future **ball-centre** target. It freezes on the first jump.
- **Height** is the real live ball-centre height before contact and the real touch/end height afterward. The physical range is 93–2044 uu.
- **Reaction Cue** is a solid bottom-centre rectangle. It is red immediately in Reaction Cue mode, becomes green at the frozen cue time, and never depends on audio or external files.

## Solver behavior

The selector first applies cheap reach bounds to the complete ball path, keeps a bounded finalist set, and only then runs profile simulation. Search uses a coarse duration pass plus local refinement and has candidate, profile-simulation, and solve-time budgets. Automatic mode falls back to a simple pre-contact reach estimate rather than leaving the HUD empty.

The advanced ball and Shoot models remain approximations. Diagnostics identify whether the selected result came from `ADVANCED` or `SIMPLE FALLBACK`; a displayed Shoot estimate is not a guaranteed goal.

## Presets

- Shoot Default
- Fast Touch Default
- Custom

Preset slot data is copied through one schema. Custom slot cvars are accessed dynamically by the generic preset loader; they are listed as dynamically used in `cvar_audit.txt`.

## Diagnostics

Open **Solver / Advanced → Diagnostics** to inspect validation state, backend, path size, candidates tested, reachable candidates, profile simulations, solve duration, cue state/times, target times, car nose/vector-to-target, signed angle, live ball height, target height, and the latest rejection reason.

## Build

GitHub Actions builds x64 C++20 and treats repository warnings as errors while isolating third-party SDK warnings.
