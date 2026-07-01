# Takeoff Coach 5.5.0 — Coherent Profiles

BakkesMod Freeplay drill for Fast Touch and Shoot.

## Runtime package
GitHub Actions publishes only `TakeoffCoach.dll` and `takeoffcoach.cfg`.

## Key behavior
- Read mode grades the latest viable jump for the selected contact.
- Reaction Cue turns green before the physical jump time by the configured human allowance.
- Ball and car setups are generated around a coherent interception route.
- Maximum bounces is a real Drill constraint; default is zero.
- Height is live before contact and freezes only on a valid local-car touch.
- Matching mode setup auto-load is enabled by default and can be disabled.
