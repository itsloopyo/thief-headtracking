# ultimate-asi-loader (vendored)

This directory contains a bundled copy of the upstream mod loader. It is the install-time
source of truth: install.cmd extracts directly from here and never reaches out to the network.
Refresh manually with `pixi run update-deps`, then commit.

## Snapshot

- Asset: `Ultimate-ASI-Loader_x64.zip`
- Tag: `v9.7.4`
- Commit: `6b440669144c4a0bef5718ab155df160d231cd42`
- Upstream URL: https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/v9.7.4/Ultimate-ASI-Loader_x64.zip
- SHA-256: `8272d83b2692662098746f2d0ad0e2d85f3c8358ab1d63f75fbe835c2c8135fd`
- dinput8.dll SHA-256: `fa266e3513d02c08a1b808f28c10538a489eaffaa4b0707f7cc1066e71b5afd7`
- Fetched at: 2026-09-02T08:14:51.9861134+01:00
- Source: github

Do not edit this directory by hand. Run ``pixi run update-deps`` to refresh, then commit.

## Bundled components

The x64 target compiles these into `dinput8.dll`, so shipping that binary is a
distribution of all of them, and each notice ships in the release ZIP:

- miniz (MIT) - `miniz-LICENSE.txt` - https://github.com/ThirteenAG/Ultimate-ASI-Loader/blob/master/external/miniz/LICENSE
- MinHook (BSD-2-Clause) - the loader compiles `external/injector/minhook`, and that
  submodule points at TsudaKageyu/minhook, the same upstream project the mod compiles
  into its own .asi. One text covers both copies, and the release ZIP ships it as
  `licenses/minhook-LICENSE.txt` - https://github.com/TsudaKageyu/minhook/blob/master/LICENSE.txt
- ThirteenAG/injector (zlib) - `external/injector/utility/FunctionHookMinHook` is the only
  part of the injector tree itself that the x64 target compiles - `injector-LICENSE.txt` -
  https://github.com/ThirteenAG/injector/blob/master/LICENSE

MemoryModule, d3d8to9 and minidx9 are Win32-only targets and are absent from
this binary. That set is a property of the release, not of the upstream
repository - re-read premake5.lua on every bump.
