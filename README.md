# Takeoff Coach 5.1 Vector Runtime Fix

BakkesMod freeplay drill for two objectives: **Fast Touch** and **Shoot**.

## Runtime systems

- Reaction grading only runs in Reaction Cue guidance.
- Cue time, target, and solution freeze on the first green cue frame.
- Candidate targets appear immediately and lock as soon as the configured stable time is satisfied; the verification window is a timeout, not a mandatory delay.
- Fast Touch ranks only reachable useful contacts. Shoot ranks only reachable contacts whose estimated outgoing ball path crosses the selected goal corridor.
- One stored contact target drives timing, alignment, ball marker, takeoff marker, cue, and grading.
- Ball prediction stores persistent last-bounce classification, uses one collision response per bounded substep, proper normal/tangent decomposition, approximate spin coupling, and rejects unsupported curved-transition regions.
- Touch and goal hooks are used first, with geometric/position fallbacks.
- Session statistics use separate grading denominators.
- Shoot Default, Fast Touch Default, and Custom store full functional settings. Objective changes do not overwrite settings unless auto-load is enabled.

## Build

GitHub Actions builds x64 C++20 with warnings treated as errors for repository-owned code. BakkesMod SDK headers are treated as external headers.

The workflow artifact contains:

- `TakeoffCoach.dll`
- `takeoffcoach.cfg`
- `install_takeoffcoach.command`
