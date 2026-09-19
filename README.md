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
4. Launch the game. `ThiefHeadTracking.ini` and `HeadTracking.log` are written next to those two files on the first run.

Use `install.cmd`, or place the two files by hand as above, rather than a mod manager. These two files have to sit beside `Shipping-ThiefGame.exe` in `Binaries2\Win64\`, and a manager deploys into one fixed folder inside the game that is not that one, so the archive installs somewhere the game never looks while the manager reports success.

To remove a manual install, delete the two files you copied, plus the INI and the log.

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
| Cycle ADS mode      | `Insert`    | `Ctrl+Shift+U`  |

`Page Up` / `Ctrl+Shift+G` cycles tracking mode: full tracking, then rotation only, then position only, then back to full.

`Page Down` / `Ctrl+Shift+H` switches head yaw between horizon-locked and camera-local. Horizon-locked is the default and keeps "up" where it is however the mouse is pitched.

`Insert` / `Ctrl+Shift+U` cycles what head tracking does while the bow is drawn, between `paused` (the game keeps the camera until you lower the bow) and `tracked` (head tracking carries on, and Thief's own crosshair keeps marking the aim point). The choice is written back to the INI, so it survives a restart. On the current build the cycle changes the setting but nothing else, because the mod cannot yet see the bow being drawn.

The mod draws no text of its own, so a mode you switch to is named in `HeadTracking.log` rather than on screen.

## Configuration

Settings live in `ThiefHeadTracking.ini`, written into `Binaries2\Win64\` next to the loader the first time the mod runs. Edit it with the game closed.

```ini
; Thief - Head Tracking configuration
; Lives next to dinput8.dll in Binaries2/Win64/.

[General]
EnableOnStartup=1
Port=4242
; Yaw mode: true = horizon-locked yaw (default), false = camera-local.
WorldSpaceYaw=1
; Projects the game's aim point into the head-tracked view.
; The reticle leaves the screen when the aim point is outside the view.
MoveCrosshair=1
; What head tracking does while the bow is drawn:
;   paused  - the game keeps the camera until you lower the bow (default)
;   tracked - head tracking carries on, and the game's own crosshair keeps
;             marking the aim point
; Cycled in game with Insert or Ctrl+Shift+U, which writes the choice back here.
AdsMode=paused

[Sensitivity]
Yaw=1
Pitch=1
Roll=1
; Flip an axis only if your tracker reports it backwards. The engine's own
; sign conventions are already handled; these three ship off.
InvertYaw=0
InvertPitch=0
InvertRoll=0

[Smoothing]
; Chosen per connection from the tracker's source address; covers rotation and position.
; LocalSmoothing: tracker running on this machine (loopback). 0 = none, 1 = heavy.
LocalSmoothing=0
; RemoteSmoothing: tracker on a remote network device. 0 = none, 1 = heavy.
RemoteSmoothing=0.15

[Position]
; 6DOF positional tracking. PositionScale = world units (cm) per metre of head translation.
Enabled=1
SensitivityX=1
SensitivityY=1
SensitivityZ=1
LimitX=0.3
LimitY=0.2
LimitZ=0.4
LimitZBack=0.1
PositionScale=100

[Collision]
; Trace positional lean against the level. Off by default.
Enabled=0
; World units (cm) kept between the camera and the surface it stopped at.
Margin=20
; How quickly the lean reopens once whatever blocked it is gone.
; 0 = instantly, 1 = very slowly. Blocking is always immediate.
ReleaseSmoothing=0.9
; Trace flags the world query is cast with. 0 uses the value for your game build.
Channel=0x00

[Hotkeys]
; Virtual-key codes. Defaults: End (toggle), Page Up (cycle tracking mode), Page Down (yaw mode), Insert (cycle ADS mode).
Toggle=0x23
CycleMode=0x21
YawMode=0x22
AdsMode=0x2D
; Chord alternatives: Ctrl+Shift+Y (toggle), Ctrl+Shift+G (cycle tracking mode), Ctrl+Shift+H (yaw mode), Ctrl+Shift+U (cycle ADS mode).
ChordToggle=1
ChordCycleMode=1
ChordYawMode=1
ChordAdsMode=1

[Diagnostics]
; Dumps the game structures this mod reads into HeadTracking.log, once each.
; Leave it off unless a bug report asks for it: it makes the log hundreds of
; lines longer and changes nothing about how the mod behaves.
StructProbe=0
```

Thief has its own field of view slider in the graphics options, so the mod adds none. It reads the field of view the game is rendering and scales head tracking to it, so your head moves the view by the same amount at any setting.

## Troubleshooting

**Mod not loading:**

- Look for `HeadTracking.log` in `Binaries2\Win64\`. No log at all means the loader never ran: confirm `dinput8.dll` and `ThiefHeadTracking.asi` are both in that folder, beside `Shipping-ThiefGame.exe`.
- The log's first lines name the game build the mod matched. If they say the build is not one it knows, the game has been patched since this release and the mod stays dormant rather than hooking a moved address.
- If your game folder is somewhere Windows protects, run `install.cmd` from an elevated prompt.

**No tracking response:**

- Confirm your tracker is running and started, with output set to UDP at `127.0.0.1:4242`.
- Check that `Port` in the INI matches the port your tracker sends to.
- Press `End` or `Ctrl+Shift+Y`. Tracking may be toggled off, and every toggle is recorded in `HeadTracking.log`.
- If a firewall prompt appeared the first time your tracker sent, allow it. A blocked sender looks exactly like no tracker at all.

**Jittery or unstable tracking:**

- A tracker on this PC sending to `127.0.0.1` uses `LocalSmoothing`, which ships at 0. Raise it toward 1 if your source is noisy.
- A phone or another machine on the network uses `RemoteSmoothing`, which ships at 0.15. Raise it for a busy WiFi link.
- Better lighting and a plainer background fix more webcam jitter than any smoothing value will.

**Wrong rotation axis:**

- If yaw feels wrong while you are looking steeply up or down, switch yaw mode with `Page Down` or `Ctrl+Shift+H`. Horizon-locked is the default; camera-local turns about the axis the camera currently points along.
- If a whole axis runs backwards, fix it in your tracker's profile. `InvertYaw`, `InvertPitch` and `InvertRoll` are there for a tracker that genuinely reports a rotation axis backwards. There is no equivalent for leaning: a mirrored lean is a tracker profile to correct, not a game setting.

**The crosshair drifts off where the arrow lands:**

- Check that `[General] MoveCrosshair=1`. The mod projects the clean aim point into the tracked view, including positional lean. If the target leaves the view, the reticle leaves it too.

**Leaning into a wall shows you what is behind it:**

- Positional collision is off by default. Set `[Collision] Enabled=1` to enable the level trace, or lower the `[Position]` limits.

## Updating

Download the new release and run `install.cmd` again. Your config is preserved.

## Uninstalling

Run `uninstall.cmd`. This removes the mod DLLs. The mod loader is only removed if the installer put it there. Use `uninstall.cmd /force` to remove it anyway.

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
