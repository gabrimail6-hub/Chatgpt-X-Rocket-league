# Takeoff Coach — BakkesMod Freeplay plugin source

This plugin isolates the part you described:

- approach distance;
- braking/releasing throttle;
- ground speed at takeoff;
- jumping too early or too late;
- lateral alignment.

It creates a repeatable aerial setup in Freeplay and grades the **first jump press**.

## Commands

Open the BakkesMod console with `F6`.

```text
tc_start
```

Starts the fixed baseline drill.

```text
tc_random
```

Rotates the setup and adds small variations.

```text
tc_reset
```

Restarts the fixed setup.

Useful CVars:

```text
tc_target_speed 1350
tc_auto_reset 1
tc_reset_delay 2.2
```

## How the grading works

The drill has a defined target takeoff point and target forward speed.

- Negative position error: you jumped early.
- Positive position error: you jumped late.
- Positive speed error: you carried too much ground speed.
- Negative speed error: you slowed down too much.
- Lateral error: your approach line was not centered.

This is intentionally a repeatable calibration drill. Version 0.1 does **not**
claim to calculate the globally optimal takeoff for every possible live ball.

## Build route recommended for your Mac/Heroic setup

The DLL is still a Windows BakkesMod plugin. The easiest route is:

1. Create a repository from the current official BakkesMod plugin template.
2. Replace the generated plugin `.h` and `.cpp` with the files in `src/`.
3. Keep the template's Visual Studio solution/project files.
4. Build with Visual Studio 2022 on Windows, or use the template's GitHub
   Actions workflow on a Windows runner.
5. Put `TakeoffCoach.dll` in the same BakkesMod `plugins` directory used by
   your working Heroic/Wine installation.
6. In the BakkesMod console:

```text
plugin load TakeoffCoach
```

Then enter Freeplay and run:

```text
tc_start
```

## Important compatibility note

I could not compile or inject-test this DLL inside Rocket League from the
current environment. The code follows the documented BakkesMod SDK interfaces,
but SDK/template revisions can require tiny include-name or field-name changes.
The most likely compatibility points are:

- the exact `ControllerInput` include path;
- `memory_address` visibility for comparing the caller with the local car;
- the overload of `SetAngularVelocity`;
- the `DrawString` overload used by the installed SDK.

If your compiler reports one of those, send the exact build error and it can be
patched directly rather than guessed.

## Suggested training progression

1. Set `tc_auto_reset 0`.
2. Repeat `tc_start` until you can land within approximately:
   - ±1.2 m longitudinally;
   - ±140 uu/s;
   - ±0.9 m laterally.
3. Set `tc_auto_reset 1`.
4. Use `tc_random`.
5. Change target speed:
   - `tc_target_speed 1100` for controlled takeoffs;
   - `tc_target_speed 1500` for faster approaches.
