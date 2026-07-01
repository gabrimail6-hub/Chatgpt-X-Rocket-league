# Takeoff Coach 4.1

A BakkesMod Freeplay plugin for training the latest viable fast-aerial takeoff without RocketSim.

## Drill objectives

- **Fast Touch**: reach the ball quickly and reports car speed at contact.
- **Control Touch**: rewards a lower car-to-ball relative contact speed.
- **Score**: computes a goal-directed contact and keeps tracking after the touch so the goal can be detected.
- **Random Call**: chooses and displays one of the three objectives for every attempt.

## Setup presets

- **Shooting**: the requested shooting/setup defaults.
- **Fast Touch**: broader position, direction, height and speed ranges, including away, crossing and toward trajectories.
- **Custom**: edit any min/max range, press **Save current as Custom**, and reload it later.

The two built-in presets remain recoverable. Saving never silently overwrites them.

## Install on macOS / Heroic

1. Download the compiled `TakeoffCoach` artifact from the latest successful GitHub Actions run.
2. Keep `TakeoffCoach.dll`, `takeoffcoach.cfg`, and `install_takeoffcoach.command` in the same folder.
3. Double-click `install_takeoffcoach.command`. If macOS blocks it, right-click it and choose **Open**.
4. Restart Rocket League/BakkesMod and open **F2 > Plugins > Takeoff Coach**.

The installer searches common Heroic and CrossOver Wine prefixes, installs the DLL and config, and adds the plugin/config load commands without duplicating them.

## Manual build

The repository includes a C++20 x64 Visual Studio project. Set the BakkesMod SDK path expected by `BakkesMod.props`, then build `TakeoffCoach.vcxproj` in Release x64. GitHub Actions also builds a ready-to-install artifact.

## Notes

The reach calculation is a configurable deterministic training model, not a full Rocket League physics simulation. Tune the horizontal/vertical calibration and reaction allowance to your mechanics.
