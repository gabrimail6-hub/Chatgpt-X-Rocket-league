# Takeoff Coach 5.0 Vector

A BakkesMod freeplay plugin with exactly two objectives: **Fast Touch** and **Shoot**.

The Vector pipeline predicts one native ball path at a fixed timestep, selects one `ContactTarget`, and uses that same locked target for the contact marker, aerial timing, takeoff marker, cue, alignment, grading, and shot evaluation. Supported prediction surfaces include the floor, ceiling, side/back walls, diagonal corners, goal interior, posts, and crossbar approximations. Bounce policy and maximum bounce count are configurable.

## Guidance

**Read** displays timing, alignment, contact, and takeoff guidance. **Reaction Cue** renders a red lamp before the ideal first-jump time and a green lamp at the cue. The first jump edge freezes the target and takeoff direction, records raw reaction time, subtracts the configured allowance (100 ms by default), and reports reaction loss and possible time saved.

## Objectives

- **Fast Touch** ignores both goals and prioritizes the earliest useful reachable touch, with configurable contact height and bounce allowance.
- **Shoot** chooses the correct attacking goal, supports centre/near-post/far-post/random/custom aim, estimates outgoing direction and speed, rejects contacts outside the goal corridor, and continues tracking after contact to detect a goal.

## Solver

The bounded human aerial solver tests single jump, fast aerial, delayed second jump, jump-hold, boost-delay, pitch-delay, and conservative profile variants. It outputs the latest viable jump, aerial duration, takeoff position/direction, boost estimate, and robustness.

## Presets

The Setup tab provides **Shoot Default**, **Fast Touch Default**, and **Custom**, with load, reset, copy-to-Custom, and save-current-to-Custom controls. Built-in defaults remain recoverable.

## Build

The Visual Studio project targets C++20 x64 and uses the BakkesMod SDK path supplied through `BAKKESMOD_SDK`. GitHub Actions builds all repository-owned `.cpp` files and packages the DLL and default config.
