# Changelog

## 5.1.0 Vector Runtime Fix

- Fixed Reaction Cue data leaking into Read guidance.
- Separated raw reaction, allowance-adjusted reaction loss, and actual contact-time loss.
- Froze cue time, target, and solution after cue activation.
- Added separate reaction target policies.
- Replaced mandatory full verification delay with early stable lock and timeout-based failure.
- Rerolls when the verification window expires without any valid reachable candidate.
- Ranks candidates only after aerial reachability testing.
- Added persistent bounce-surface classification and observed-vs-predicted bounce validation.
- Corrected normal/tangential bounce decomposition and added bounded collision substeps.
- Marked unsupported curved-transition regions instead of producing confident targets through them.
- Added practical spin coupling in bounce response.
- Replaced fabricated Shoot aim scoring with a basic car-ball impulse and goal-corridor estimate.
- Improved near/far-post selection from approach geometry.
- Simulated grounded travel and bounded car orientation before aerial contact.
- Split ball-path, car-contact, and takeoff-marker tolerances.
- Improved touch and goal detection with event hooks and fallbacks.
- Corrected session statistic denominators and absolute angle averaging.
- Expanded preset memory and removed objective-change preset overwrite.
- Fixed direct preset copying and Custom reset side effects.
- Added Cross, Square, and Ring marker shapes and camera-facing projection checks.
- Standardized ball-centre height controls to 93–2044 uu.
