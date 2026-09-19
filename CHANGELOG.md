# Changelog

## [0.0.0] - 2026-09-06

### Added
- Added head tracking for Thief. The view follows your head while the game's own camera is left exactly where it put it, so aim, interaction traces and what the guards can see are the same whether tracking is on or off.
- Head tracking continues through cutscenes and animated camera movement. The main menu, loading screens and the pause menu keep the game's camera.
- Head roll now tilts the view the same way you tilt your head.
- Quest markers, guard awareness indicators and the `[E]` pick-up prompt stay on the thing they mark when you turn or lean your head. Off-screen quest directions follow head rotation too.
- Added 6DOF positional tracking, with per-axis limits in the INI. Leaning is limited by those numbers rather than by the walls around you, so lean gently near geometry.
- The bow reticle follows the clean aim point as you turn or lean your head. Targets outside the view no longer produce a reticle at screen centre.
- Added an aim mode setting for the drawn bow, on `Insert` / `Ctrl+Shift+U`, between `paused` and `tracked`. The choice is saved, but this release cannot yet tell when the bow is drawn, so head tracking carries on either way.
- Added field of view compensation, read fresh every frame, so a narrowed field of view no longer makes the same head movement swing the view further. Head yaw, head pitch and leaning are scaled; head roll is not.
- Added world-locked and camera-local yaw, with `WorldSpaceYaw` in the INI and `Page Down` / `Ctrl+Shift+H` to switch between them while you play.
- Added `End` / `Ctrl+Shift+Y` to toggle tracking and `Page Up` / `Ctrl+Shift+G` to cycle rotation and position together, then rotation only, then position only.
- Added two smoothing settings, `LocalSmoothing` and `RemoteSmoothing`, chosen per connection from the address the tracker packets arrive from and covering rotation and position alike.
- Added dormancy on an unrecognised game build: after a game patch the mod installs no hooks and the game runs exactly as it did before, with the reason written to the log.
- Added a self-documenting `ThiefHeadTracking.ini`, written beside the game executable the first time the mod runs.
- Added `HeadTracking.log` beside the game executable, with one previous generation kept as `HeadTracking.prev.log`.
- Added window centring, so windowed play starts with the game window centred on the monitor it opened on.
