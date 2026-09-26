# Changelog

## [Unreleased]

### Added

- A setting set to `default` in `CameraUnlock.ini` takes its value from `Defaults.ini`, which every head tracking mod that keeps its settings in `CameraUnlock.ini` reads. Head tracking mods that keep their settings in another file do not read it, and neither do earlier versions of this mod. Writing a value in place of `default` changes that setting for this game only. When the mod saves a setting that a hotkey changed in game, it writes the new value in place of `default`, so that setting no longer follows `Defaults.ini` in this game until you set it to `default` again.
- `Defaults.ini` is `%AppData%\CameraUnlock\Defaults.ini` on Windows; `$XDG_CONFIG_HOME/CameraUnlock/Defaults.ini` on Linux, or `~/.config/CameraUnlock/Defaults.ini` where `XDG_CONFIG_HOME` is not set, under Wine and Proton too; and `~/Library/Application Support/CameraUnlock/Defaults.ini` on macOS. The mod's log, where it writes one, names the file it read.
- When the mod starts and finds no `Defaults.ini`, it creates one holding the built-in values, unless Windows runs the game as a packaged app. The mod never changes `Defaults.ini` after that.

### Changed

- Settings move to `Binaries2\Win64\CameraUnlock.ini`. Earlier versions of the mod kept these settings in `ThiefHeadTracking.ini`, in the same folder. The first time this version starts and finds no `CameraUnlock.ini`, it reads your settings from `ThiefHeadTracking.ini` and writes them into `CameraUnlock.ini`. It never changes `ThiefHeadTracking.ini`, and does not read it again while `CameraUnlock.ini` exists.
- A setting that the defaults the README shows set to `default` is written as `default` when the value imported for it equals its default at that start, which is the value `Defaults.ini` gives it, or the built-in value where `Defaults.ini` gives none. It then follows `Defaults.ini`. Every other setting is written with the value imported for it.
- `RotationEnabled` and `PositionEnabled` are one setting here, the tracking mode, so both are written as `default` or neither is.
- Comments, and keys the mod never read, are not carried over. Nor are these, where your old file had them:
  - A sensitivity, scale, deadzone, response curve or axis inversion you changed from its default. Set these in your tracker instead.
  - Reticle settings, and a key that toggled the reticle.
  - The setting for a feature that earlier versions shipped switched off while it was untested. It now follows the mod's default.
- An older version of the mod reads `ThiefHeadTracking.ini` and never reads `CameraUnlock.ini`, so a setting you change after updating is not in `ThiefHeadTracking.ini`.
- Deleting only `CameraUnlock.ini` makes the next start read `ThiefHeadTracking.ini` again. To go back to the defaults, replace everything in `CameraUnlock.ini` with the defaults the README shows. Every setting they set to `default` then follows `Defaults.ini`.
- Hotkeys are written as key names, and each hotkey lists every key that triggers it, the Ctrl+Shift chord included: `ToggleKey=End, Ctrl+Shift+Y`. The import carries over each key you had bound and each chord you had switched on or off, and now each one can be changed or removed like any other key.
- Settings are renamed in `CameraUnlock.ini`: `[General] Port` is `[Network] UdpPort`; the `[Position]` limits are `PositionLimitX`, `PositionLimitY`, `PositionLimitYDown`, `PositionLimitZ` and `PositionLimitZBack`; `[Collision] Enabled`, `Margin`, `ReleaseSmoothing` and `Channel` are `[Position] CollisionEnabled`, `CollisionMargin`, `CollisionReleaseSmoothing` and `CollisionChannel`; and `[Hotkeys] Toggle`, `CycleMode` and `YawMode` with `ChordToggle`, `ChordCycleMode` and `ChordYawMode` are `ToggleKey`, `CycleTrackingModeKey` and `YawModeKey`. `LimitY` bounded both directions, so it becomes both `PositionLimitY` and `PositionLimitYDown`, which can now be set apart. `[Position] Enabled` chose the tracking mode at startup; that is now the pair `RotationEnabled` and `PositionEnabled`. `CollisionChannel` is written as a decimal number where `Channel` was hexadecimal, and a flag word above `0x7FFFFFFF` is written as the negative number with the same 32 bits. The import carries every one of these values over.
- The lean collision check (`CollisionEnabled`) is now on by default, and an imported `[Collision] Enabled` is not carried over: it follows the default like any feature that earlier versions shipped switched off while it was untested. `CollisionMargin` keeps the 20 centimetres earlier versions shipped, and a margin you set is carried over.
- The tracking mode that Page Up or Ctrl+Shift+G selects, and the yaw mode that Page Down or Ctrl+Shift+H selects, are now saved to `CameraUnlock.ini` as soon as you change them and come back at the next start. End still changes the current session only.
- `uninstall.cmd` keeps `Binaries2\Win64\CameraUnlock.ini` and `Binaries2\Win64\ThiefHeadTracking.ini`, so your settings survive a reinstall. Earlier versions deleted `ThiefHeadTracking.ini` on uninstall.
- A `ThiefHeadTracking.ini` whose position limits include one above 10 is not imported, because `CameraUnlock.ini` cannot hold that value. The mod runs on the file's values with the same exceptions as an imported file: a sensitivity, scale or inversion you changed is not applied, the game's reticle follows the aim, and the lean collision check follows its default. It creates no `CameraUnlock.ini`, saves nothing that session, and says so in the log at every start until the value is fixed.
- A `ThiefHeadTracking.ini` that earlier versions refused to start with, a `[General] Port` outside 1024 to 65535, or a game folder whose path neither the ANSI code page nor an 8.3 short name can spell within 260 characters, is not imported either, and this version does not start until it is fixed, as earlier versions did not. A `UdpPort` in `CameraUnlock.ini` takes any port from 1 to 65535.
- When `CameraUnlock.ini` cannot be created, for example because `Binaries2\Win64` cannot be written, the mod runs on the settings it read from `ThiefHeadTracking.ini`, or on its defaults where there is none, saves nothing that session, and tries again at the next start. Earlier versions did not start at all when there was no `ThiefHeadTracking.ini` and they could not create one.
- Since the `dev` build of 2026-09-19, `[General] AdsMode`, `[Hotkeys] AdsMode` and `[Hotkeys] ChordAdsMode` are no longer read, and neither Insert nor Ctrl+Shift+U cycles an aim mode for the drawn bow: head tracking carries on through the draw in every case, and the lean eases out while the bow is drawn (f74fcee).

### Removed

- `[General] MoveCrosshair`. The game's bow reticle always follows the aim now, and no setting turns that off.
- The sensitivity, scale and axis inversion settings: `[Sensitivity] Yaw`, `Pitch`, `Roll`, `InvertYaw`, `InvertPitch` and `InvertRoll`, and `[Position] SensitivityX`, `SensitivityY`, `SensitivityZ` and `PositionScale`. Set these in your tracker app instead. Every one shipped at its identity value, and the 100 world units per metre the mod shipped as `PositionScale` is now part of its own axis conversion, so with these settings at their shipped values the camera moves as it did before.

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
