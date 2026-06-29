# Takeoff Coach 2

A BakkesMod Freeplay plugin for training the latest viable fast-aerial takeoff.

## What changed

- The car starts with configurable ground speed.
- The ball starts with configurable forward and upward velocity.
- Random mode changes angle, speed, and lateral offset.
- The coaching cue recalculates from the live ball position and velocity.
- The HUD shows WAIT, ALIGN, JUMP NOW, or TOO LATE.
- The first jump is graded for timing and direction.
- All important values are adjustable in F2 > Plugins > Takeoff Coach.

## Use

1. Load `TakeoffCoach.dll`.
2. Enter Freeplay.
3. Open F2 > Plugins > Takeoff Coach.
4. Press **Start randomized drill**.
5. Stay grounded while the HUD says WAIT, line up while it says ALIGN, and fast aerial when it says JUMP NOW.

The reach calculation is a configurable training model, not a perfect Rocket League physics solver. Tune **How late to wait** and the boost-efficiency sliders until the JUMP NOW cue matches takeoffs that are barely reachable for your mechanics.


<!-- V3 fixed rebuild: 2026-06-29 18:47:43 UTC -->

<!-- Takeoff Coach 3.2 height-gauge update: 2026-06-29 19:32:13 UTC -->

<!-- Takeoff Coach 3.3: 2026-06-29 20:31:20 UTC -->

<!-- Takeoff Coach 3.3.1 solver repair: 2026-06-29 20:35:25 UTC -->

<!-- Takeoff Coach 3.3.2 live locked-target guidance: 2026-06-29 20:52:23 UTC -->
