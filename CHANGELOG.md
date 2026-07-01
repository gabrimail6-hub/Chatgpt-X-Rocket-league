# Changelog

## 5.0.0 Vector

- Reduced objectives to Fast Touch and Shoot; renamed Score to Shoot everywhere.
- Added a shared fixed-step native ball path and a single locked ContactTarget.
- Added supported surface bounces, bounce policy, damping, restitution, and goal geometry approximations.
- Added bounded human aerial profiles and takeoff-position/facing output.
- Implemented Reaction Cue rendering and reaction grading.
- Implemented candidate/locked contact marker and takeoff marker/arrow/target line.
- Replaced candidate-to-candidate drift verification with snapshot path validation and correction.
- Added Shoot aim modes, outgoing direction/speed estimation, corridor rejection, and post-touch goal tracking.
- Applied the requested Shoot defaults consistently in C++ and configuration.
- Added session statistics and a Solver / Advanced tab.
- Removed all non-Fast-Touch and non-Shoot objective branches and settings.
