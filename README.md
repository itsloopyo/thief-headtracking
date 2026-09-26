# Thief Head Tracking

![Thief running with this mod](https://raw.githubusercontent.com/itsloopyo/thief-headtracking/main/assets/readme-clip.gif)

An unofficial head tracking mod for Thief that moves the view with your head while your mouse or controller keeps aiming, driven by a webcam, phone, or any OpenTrack compatible tracker, with no VR headset required.

## Features

- **Decoupled look and aim** - your head moves the view while the mouse or controller still controls where you aim.
- **6DOF tracking** - lean in for a closer look, or peer around a corner, and the view follows your head.
- **Works with any OpenTrack compatible tracker** - free options available for PC, iOS and Android

## Requirements

- [Thief](https://store.steampowered.com/app/239160/Thief/) on Steam.
- A tracking source that sends the OpenTrack UDP protocol: [OpenTrack](https://github.com/opentrack/opentrack) with a webcam or VR headset, or a phone app that speaks the protocol itself.
- Windows 10 or 11, 64-bit. The mod loads into the game's 64-bit executable.

## Installation

### Standalone Installer

1. Download `ThiefHeadTracking-v<version>-installer.zip` from the [Releases page](https://github.com/itsloopyo/thief-headtracking/releases).
2. Extract it anywhere.
3. Double-click `install.cmd`.
4. Configure OpenTrack to output UDP to `127.0.0.1:4242`.
5. Launch the game.

If the installer cannot find your copy of the game, point it at the install folder yourself. Either set the environment variable:

```powershell
$env:THIEF_PATH = "D:\Games\Thief"
.\install.cmd
```

or pass the folder as an argument:

```powershell
.\install.cmd "D:\Games\Thief"
```

Both want the folder that contains `Binaries2\Win64\Shipping-ThiefGame.exe`, not the executable itself.

### Manual Installation

The installer copies two files and nothing else, so you can place them by hand.

1. Open `Binaries2\Win64\` inside your Thief install folder. `Shipping-ThiefGame.exe` is in there.
2. From the installer ZIP, copy `vendor\ultimate-asi-loader\dinput8.dll` into that folder. This is Ultimate ASI Loader, renamed to a DLL the game already imports so Windows loads it.
3. From the same ZIP, copy `plugins\ThiefHeadTracking.asi` into that folder, beside the loader.
4. Launch the game. `CameraUnlock.ini` and `HeadTracking.log` are written next to those two files on the first run.

Use `install.cmd`, or place the two files by hand as above, rather than a mod manager. These two files have to sit beside `Shipping-ThiefGame.exe` in `Binaries2\Win64\`, and a manager deploys into one fixed folder inside the game that is not that one, so the archive installs somewhere the game never looks while the manager reports success.

To remove a manual install, delete the two files you copied and `HeadTracking.log`. `CameraUnlock.ini` holds your settings, so leave it if you may install the mod again.

## Setting Up OpenTrack

In OpenTrack, set **Output** to `UDP over network`, open its options, and set the address to `127.0.0.1` and the port to `4242`. Pick your **Input** to match your hardware, then press **Start**.

Centering is done in your tracker, not in the game: OpenTrack's Center bind, SteamVR's reset, or the CENTER button in your phone app.

### VR Headset Setup

1. Connect the headset to the PC over Air Link, Virtual Desktop, or a link cable.
2. Start SteamVR and let it see the headset.
3. In OpenTrack, set **Input** to the SteamVR tracker.
4. Leave **Output** on `UDP over network`, `127.0.0.1:4242`.

### Webcam Setup

1. In OpenTrack, set **Input** to `neuralnet tracker`. It tracks your face from an ordinary webcam and needs no markers, clips, or IR hardware.
2. Open its options and pick your camera and resolution.
3. Leave **Output** on `UDP over network`, `127.0.0.1:4242`.

### Phone App Setup

The mod accepts one thing: the OpenTrack UDP protocol on port `4242`. A phone tracker is usable here if it sends that protocol itself, or ships a PC-side companion that does. Plenty of phone trackers speak something else entirely, so check your app against that first.

For an app that does send it, what decides the wiring is how much filtering the app does before the packet leaves the phone. An app that filters on-device can point straight at this PC's LAN address on port `4242`. A raw or lightly filtered feed sent direct will jitter, because the mod's smoothing is sized to take the edge off a clean signal rather than to rescue a noisy one, and that app should go through OpenTrack instead so its filters and curves can clean the feed up first.

The test is quicker than the reading: send direct, hold your head still, and if the view drifts or shakes, route it through OpenTrack.

I made [Headcam](https://headcam.app) so decent tracking was free for anybody with a phone already in their pocket. It filters on-device, so it can send directly. Any app that filters enough noise works exactly as well.

A phone on WiFi is a remote connection and gets `RemoteSmoothing`. So does a tracker running on this same PC if you point it at this machine's LAN address instead of `127.0.0.1`, because the mod classifies the transport rather than the machine. Send to `127.0.0.1` to get `LocalSmoothing`.

## Controls

Two equivalent binding sets - use whichever your keyboard has:

| Action              | Nav-cluster | Chord           |
|---------------------|-------------|-----------------|
| Toggle tracking     | `End`       | `Ctrl+Shift+Y`  |
| Cycle tracking mode | `Page Up`   | `Ctrl+Shift+G`  |
| Toggle yaw mode     | `Page Down` | `Ctrl+Shift+H`  |

`Page Up` / `Ctrl+Shift+G` cycles tracking mode: full tracking, then rotation only, then position only, then back to full.

`Page Down` / `Ctrl+Shift+H` switches head yaw between horizon-locked and camera-local. Horizon-locked is the default and keeps "up" where it is however the mouse is pitched.

The tracking mode and the yaw mode are saved to `CameraUnlock.ini` as soon as you change them, and come back at the next start. `End` / `Ctrl+Shift+Y` changes the current session only: whether tracking is on at startup is `EnableOnStartup`.

Each action's keys are a list in the `[Hotkeys]` section of `CameraUnlock.ini`, the chord included, so any of them can be rebound or removed.

The mod draws no text of its own, so a mode you switch to is named in `HeadTracking.log` rather than on screen.

### Aiming the bow

Head tracking stays on while you draw the bow. The bow stays where your mouse or controller points it, so with your head turned it sits off to one side with its aim still lined up, and your arrows land where it points. Head movement is scaled to the zoom, so the draw does not magnify it.

Leaning eases out while the bow is drawn, because it would move your eye off the arrow's line. On the current build the mod cannot yet see the bow being drawn, so the lean stays in.

## Configuration

Apart from creating `CameraUnlock.ini` at startup when there is none, the mod writes to it only when a hotkey changes the tracking mode or the yaw mode. It never writes `ThiefHeadTracking.ini` or `Defaults.ini`. Edit `CameraUnlock.ini` with the game closed.

<!-- cameraunlock:config -->
The mod reads its settings from `Binaries2\Win64\CameraUnlock.ini` in the game folder, and creates the file when it starts and finds none. Edit it with any text editor.

A setting set to `default` takes its value from `Defaults.ini`, which every head tracking mod that keeps its settings in `CameraUnlock.ini` reads. Head tracking mods that keep their settings in another file do not read it, and neither do earlier versions of this mod. Writing a value in place of `default` changes that setting for this game only. When the mod saves a setting that a hotkey changed in game, it writes the new value in place of `default`, so that setting no longer follows `Defaults.ini` in this game until you set it to `default` again.

`Defaults.ini` is `%AppData%\CameraUnlock\Defaults.ini` on Windows; `$XDG_CONFIG_HOME/CameraUnlock/Defaults.ini` on Linux, or `~/.config/CameraUnlock/Defaults.ini` where `XDG_CONFIG_HOME` is not set, under Wine and Proton too; and `~/Library/Application Support/CameraUnlock/Defaults.ini` on macOS. The mod's log, where it writes one, names the file it read.

When the mod starts and finds no `Defaults.ini`, it creates one holding the built-in values, unless Windows runs the game as a packaged app. The mod never changes `Defaults.ini` after that. Edit it with any text editor.

Earlier versions of the mod kept these settings in `ThiefHeadTracking.ini`, in the same folder. The first time this version starts and finds no `CameraUnlock.ini`, it reads your settings from `ThiefHeadTracking.ini` and writes them into `CameraUnlock.ini`. It never changes `ThiefHeadTracking.ini`, and does not read it again while `CameraUnlock.ini` exists.

A setting that the defaults below set to `default` is written as `default` when the value imported for it equals its default at that start, which is the value `Defaults.ini` gives it, or the built-in value where `Defaults.ini` gives none. It then follows `Defaults.ini`. Every other setting is written with the value imported for it. `RotationEnabled` and `PositionEnabled` are one setting here, the tracking mode, so both are written as `default` or neither is.

Comments, and keys the mod never read, are not carried over. Nor are these, where your old file had them:

- Reticle settings, and a key that toggled the reticle.
- A sensitivity, scale, deadzone, response curve or axis inversion you changed from its default. Set these in your tracker instead.
- The setting for a feature that earlier versions shipped switched off while it was untested. It now follows the mod's default.

An older version of the mod reads `ThiefHeadTracking.ini` and never reads `CameraUnlock.ini`, so a setting you change after updating is not in `ThiefHeadTracking.ini`.

Deleting only `CameraUnlock.ini` makes the next start read `ThiefHeadTracking.ini` again. To go back to the defaults, replace everything in `CameraUnlock.ini` with the defaults below. Every setting they set to `default` then follows `Defaults.ini`.

The built-in value of each setting set to `default` below:

- `UdpPort=4242`
- `EnableOnStartup=true`
- `WorldSpaceYaw=true`
- `RotationEnabled=true`
- `LocalSmoothing=0.0`
- `RemoteSmoothing=0.15`
- `PositionEnabled=true`
- `PositionLimitX=0.3`
- `PositionLimitY=0.2`
- `PositionLimitYDown=0.2`
- `PositionLimitZ=0.4`
- `PositionLimitZBack=0.1`
- `CollisionEnabled=true`
- `CollisionReleaseSmoothing=0.9`
- `ToggleKey=End, Ctrl+Shift+Y`
- `CycleTrackingModeKey=PageUp, Ctrl+Shift+G`
- `YawModeKey=PageDown, Ctrl+Shift+H`

With every setting at its default, the file reads:

```ini
; Thief head tracking settings.
; Comments start with ; and go on their own line. Text after a value is part of the value.
; Hotkeys are key names such as End, PageUp or Ctrl+Shift+Y. Separate several with commas; leave empty for none.
; A setting set to default takes its value from Defaults.ini, which every head tracking mod
; that keeps its settings in CameraUnlock.ini reads: %AppData%\CameraUnlock\Defaults.ini on
; Windows, $XDG_CONFIG_HOME/CameraUnlock/Defaults.ini (normally ~/.config/CameraUnlock) on
; Linux, under Wine and Proton too, and ~/Library/Application Support/CameraUnlock/Defaults.ini
; on macOS. The log names the file it read. Write a value instead of default to change that
; setting for this game only.

[CameraUnlock]
; Written by the mod. Leave this section in place.
ConfigFormat=1

[Network]
; UDP port the mod receives tracker data on (OpenTrack protocol).
UdpPort=default

[General]
; true: head tracking is on when the game starts. ToggleKey turns it on and off.
EnableOnStartup=default
; true: yaw turns around the world's up axis. false: around the camera's own up axis.
WorldSpaceYaw=default
; true: turning your head turns the view.
; Tracking mode at startup, with PositionEnabled. The mode hotkey changes both.
RotationEnabled=default

[Smoothing]
; Smoothing when the tracker runs on this PC. 0 is the least, 1 the most.
LocalSmoothing=default
; Smoothing when the tracker is another device on the network, such as a phone.
; 0 is the least, 1 the most.
RemoteSmoothing=default

[Position]
; true: moving your head moves the view.
; Tracking mode at startup, with RotationEnabled. The mode hotkey changes both.
PositionEnabled=default
; How far, in metres, leaning left or right can move the view.
PositionLimitX=default
; How far, in metres, raising your head can move the view.
PositionLimitY=default
; How far, in metres, lowering your head can move the view.
PositionLimitYDown=default
; How far, in metres, leaning forward can move the view.
PositionLimitZ=default
; How far, in metres, leaning back can move the view.
PositionLimitZBack=default
; true: leaning stops at walls instead of moving the view through them.
CollisionEnabled=default
; How far, in centimetres, the view is held off a wall when you lean into it.
; CollisionMargin=20.0
; The trace flags the wall check is cast with, as a decimal number.
; 0 uses the flags pinned for your game build.
; CollisionChannel=0
; How gently the view eases back out after a wall stopped a lean.
; 0 is the quickest, 1 the slowest.
CollisionReleaseSmoothing=default

[Hotkeys]
; Turns head tracking on and off.
ToggleKey=default
; Changes the tracking mode: rotation and position, rotation only, position only.
CycleTrackingModeKey=default
; Switches yaw between the world's up axis and the camera's own (WorldSpaceYaw).
YawModeKey=default

[Diagnostics]
; true: write the game structures this mod reads to HeadTracking.log, once each.
; Leave it off unless a bug report asks for it: it makes the log hundreds of lines longer.
StructProbe=false
```
<!-- /cameraunlock:config -->

There are no sensitivity, inversion or scale settings: the mod applies the pose your tracker sends, so set those in the tracker. Leaning converts at 100 game units (centimetres) per metre of head movement, the value earlier versions shipped as `PositionScale`.

`CollisionMargin` is how far, in centimetres, a lean stops short of a wall. `CollisionChannel` is the trace flag word the wall check is cast with, written as a decimal number; `0` uses the flags pinned for your game build.

Thief has its own field of view slider in the graphics options, so the mod adds none. It reads the field of view the game is rendering and scales head tracking to it, so your head moves the view by the same amount at any setting.

## Troubleshooting

**Mod not loading:**

- Look for `HeadTracking.log` in `Binaries2\Win64\`. No log at all means the loader never ran: confirm `dinput8.dll` and `ThiefHeadTracking.asi` are both in that folder, beside `Shipping-ThiefGame.exe`.
- The log's first lines name the game build the mod matched. If they say the build is not one it knows, the game has been patched since this release and the mod stays dormant rather than hooking a moved address.
- If your game folder is somewhere Windows protects, run `install.cmd` from an elevated prompt.

**No tracking response:**

- Confirm your tracker is running and started, with output set to UDP at `127.0.0.1:4242`.
- Check that `UdpPort` in `CameraUnlock.ini`, or in `Defaults.ini` where it says `default`, matches the port your tracker sends to.
- Press `End` or `Ctrl+Shift+Y`. Tracking may be toggled off, and every toggle is recorded in `HeadTracking.log`.
- If a firewall prompt appeared the first time your tracker sent, allow it. A blocked sender looks exactly like no tracker at all.

**Jittery or unstable tracking:**

- A tracker on this PC sending to `127.0.0.1` uses `LocalSmoothing`, which ships at 0. Raise it toward 1 if your source is noisy.
- A phone or another machine on the network uses `RemoteSmoothing`, which ships at 0.15. Raise it for a busy WiFi link.
- Better lighting and a plainer background fix more webcam jitter than any smoothing value will.

**Wrong rotation axis:**

- If yaw feels wrong while you are looking steeply up or down, switch yaw mode with `Page Down` or `Ctrl+Shift+H`. Horizon-locked is the default; camera-local turns about the axis the camera currently points along.
- If a whole axis runs backwards, fix it in your tracker's profile. The mod has no axis inversion settings.

**The crosshair drifts off where the arrow lands:**

- The mod moves the game's bow reticle onto the clean aim point in the tracked view, including positional lean, and there is no setting that turns this off. If the target leaves the view, the reticle leaves it too.

**The bow is off to one side when I draw it:**

- Your head is turned: the bow stays on your aim and you are looking past it. Turn back to it, or move your aim to where you are looking.

**Leaning into a wall shows you what is behind it:**

- The lean is traced against the level while `CollisionEnabled` is `true`, which is the built-in value. `HeadTracking.log` says at startup whether the trace is on for your game build. If a lean still goes through a wall, lower the `[Position]` limits.

## Updating

Download the new release and run `install.cmd` again. Your config is preserved.

## Uninstalling

Run `uninstall.cmd`. This removes the mod DLLs. The mod loader is only removed if the installer put it there. Use `uninstall.cmd /force` to remove it anyway. `CameraUnlock.ini` and `ThiefHeadTracking.ini` are left in `Binaries2\Win64\`, so your settings survive a reinstall.

## Building from Source

Needs Visual Studio 2022 with the Desktop C++ workload, CMake, [pixi](https://pixi.sh), and [Node.js](https://nodejs.org) for the two package validators `pixi run package` finishes with.

```powershell
git clone --recursive https://github.com/itsloopyo/thief-headtracking.git
cd thief-headtracking
pixi run build-release
pixi run test
pixi run package
```

`pixi run package` writes the installer ZIP into `release/`. `pixi run install` deploys a build straight into the game folder.

## Community & Support

- [Discord](https://discord.com/invite/dxyZdyFNT9) - setup help, bug reports, and new-release announcements
- [Lopari](https://lopari.app) - free Windows launcher with one-click install and launch of head-tracking mods
- [Headcam](https://headcam.app) - free app that turns your phone into a head tracker

## License

MIT License - see [LICENSE](LICENSE) for details.

## Credits

- **Eidos-Montreal and Square Enix** for Thief.
- **[Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader)** by ThirteenAG, vendored and shipped as the loader.
- **[OpenTrack](https://github.com/opentrack/opentrack)** for the tracking protocol this mod listens on.
- **[MinHook](https://github.com/TsudaKageyu/minhook)** by Tsuda Kageyu, compiled in for the function hooks.
- **cameraunlock-core**, the shared head tracking library behind this and the other mods in the family.

See [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md) for the full notices.

## Disclaimer

This mod is not affiliated with, endorsed by, or supported by Eidos-Montreal or Square Enix. Use at your own risk.
