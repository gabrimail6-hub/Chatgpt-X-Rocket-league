# Takeoff Coach 5.6.0 — Coherent Profiles

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


## 5.6 setup additions

- Shoot vertical speed now starts at 0 uu/s by default.
- Full Random setup spans -1800 to 6000 uu approach distance, full lateral/rotation ranges, and signed car speed from -1800 to 2300 uu/s.
- Car velocity angle was removed; velocity is aligned to the intended approach line, with negative speed representing reverse motion.
- Minimum cue lead is configurable up to 3000 ms. Reaction allowance is configurable up to 1000 ms and is added on top of the lead requirement.
- The main HUD panel and reaction cue use rounded corners.
