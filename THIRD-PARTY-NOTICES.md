# Third-Party Notices

ThiefHeadTracking bundles, statically links, or credits the third-party components
listed below. Each remains the property of its authors and is used under its own
licence. Where a licence requires the copyright notice, the conditions and the
disclaimer to accompany a binary distribution, the full text is reproduced here,
and this file ships at the root of every release ZIP we publish.

No part of this repository is derived from Thief's code, and none of its extracted
assets or data files are redistributed here.

| Component | Version | Licence | How it ships |
|-----------|---------|---------|--------------|
| Ultimate ASI Loader | v9.7.4 | MIT | Bundled in the release ZIPs as `dinput8.dll` |
| injector | commit `3a384e8d1b575c09383b0fab8bd92e34cb654949`, as pinned by Ultimate ASI Loader v9.7.4 | Zlib | Compiled into the vendored `dinput8.dll` |
| miniz | as vendored in Ultimate ASI Loader v9.7.4 | MIT | Compiled into the vendored `dinput8.dll` |
| MinHook | v1.3.4, modified (in `cameraunlock-core/vendor/minhook`) | BSD-2-Clause | Compiled into `ThiefHeadTracking.asi` |
| MinHook | commit `d94c64d32ea37bc4f5ee47d580709f70c6fb6080`, as pinned by injector | BSD-2-Clause | Compiled into the vendored `dinput8.dll` |
| cameraunlock-core | `def74d7107d1823340931cbc41e474cc652826f5` | MIT | Compiled into `ThiefHeadTracking.asi` |
| OpenTrack | n/a | ISC | Not bundled; UDP protocol interoperability only |

---

## Ultimate ASI Loader

- **Version:** v9.7.4, commit `6b440669144c4a0bef5718ab155df160d231cd42`, from the
  `Ultimate-ASI-Loader_x64.zip` release asset recorded in
  `vendor/ultimate-asi-loader/README.md`
- **License:** MIT
- **Upstream:** https://github.com/ThirteenAG/Ultimate-ASI-Loader
- **Usage:** Loads `ThiefHeadTracking.asi` into the running game. It is vendored at
  `vendor/ultimate-asi-loader/`, which is the install-time source of truth:
  `install.cmd` extracts from there and never reaches the network.
- **Bundled:** yes. Shipped in the release ZIPs as `dinput8.dll`, with the upstream
  licence file beside it and again under `licenses/`.

```
MIT License

Copyright (c) 2023 ThirteenAG

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## injector

- **Version:** commit `3a384e8d1b575c09383b0fab8bd92e34cb654949`, the commit Ultimate ASI
  Loader v9.7.4 pins at `external/injector/`
- **License:** Zlib
- **Upstream:** https://github.com/ThirteenAG/injector
- **Usage:** `external/injector/utility/FunctionHookMinHook.hpp` and `.cpp` are the only
  files from injector's own tree the loader's x64 target compiles, and the RTTI name
  `FunctionHookMinHook` is present in the vendored binary. Nothing in this repository
  calls or links it; it ships only inside that binary.
- **Bundled:** yes. Compiled into the vendored `dinput8.dll`, with its licence text
  shipped as `licenses/injector-LICENSE.txt` and beside the DLL.
- **The loader's MinHook arrives through it.** The x64 target also compiles
  `external/injector/minhook/`, which is injector's own submodule. Read out of injector's
  tree at the pinned commit rather than assumed: that submodule points at
  https://github.com/TsudaKageyu/minhook and resolves to commit
  `d94c64d32ea37bc4f5ee47d580709f70c6fb6080`. It is the same upstream project, under the
  same BSD-2-Clause terms and the same two copyright holders, as the copy this repository
  compiles into `ThiefHeadTracking.asi`, so the text reproduced under MinHook below - and
  shipped as `licenses/minhook-LICENSE.txt` - is the notice for both copies.

```
Copyright (C) 2012-2014 LINK/2012 <dma_2012@hotmail.com>

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
```

---

## miniz

- **Version:** as vendored at `external/miniz/` in Ultimate ASI Loader v9.7.4
- **License:** MIT
- **Upstream:** https://github.com/richgel999/miniz
- **Usage:** Zip reading inside the loader. Nothing in this repository calls or
  links it; it ships only inside that binary.
- **Bundled:** yes. Compiled into the vendored `dinput8.dll`, with its licence text
  shipped as `licenses/miniz-LICENSE.txt`.

```
Copyright 2013-2014 RAD Game Tools and Valve Software
Copyright 2010-2014 Rich Geldreich and Tenacious Software LLC

All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```

---

## MinHook

- **Version:** v1.3.4, vendored and locally modified at
  `cameraunlock-core/vendor/minhook`. The local change is recorded in that
  directory's `LOCAL-CHANGES.md`; every other file is byte-identical to the tag.
- **License:** BSD-2-Clause
- **Upstream:** https://github.com/TsudaKageyu/minhook
- **Usage:** Installs the trampoline hooks the mod places on the game's camera,
  crosshair and window functions.
- **Bundled:** yes. Compiled into `ThiefHeadTracking.asi`, with the full upstream
  licence text shipped as `licenses/minhook-LICENSE.txt`.
- **A second copy ships inside the loader.** Ultimate ASI Loader's x64 target compiles
  injector's `minhook` submodule, which is this same upstream project at commit
  `d94c64d32ea37bc4f5ee47d580709f70c6fb6080`. Its `LICENSE.txt` carries the same terms and
  the same two copyright holders as the text below, so `licenses/minhook-LICENSE.txt`
  discharges the notice for the copy in `dinput8.dll` as well as for the copy in our
  `.asi`. See the injector section above.

MinHook carries the Hacker Disassembler Engine at `src/hde/`, also BSD-2-Clause,
copyright (c) 2008-2009 Vyacheslav Patkov. Its notice is part of the licence text
reproduced in full below, and of the copy that ships beside the binary.

```
MinHook - The Minimalistic API Hooking Library for x64/x86
Copyright (C) 2009-2017 Tsuda Kageyu.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

================================================================================
Portions of this software are Copyright (c) 2008-2009, Vyacheslav Patkov.
================================================================================
Hacker Disassembler Engine 32 C
Copyright (c) 2008-2009, Vyacheslav Patkov.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

-------------------------------------------------------------------------------
Hacker Disassembler Engine 64 C
Copyright (c) 2008-2009, Vyacheslav Patkov.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

---

## cameraunlock-core

Git submodule at `cameraunlock-core/`, compiled into `ThiefHeadTracking.asi`. Our
own code, MIT licensed under a copyright line of its own, reproduced here so the
notices are complete.

- **Version:** pinned commit `def74d7107d1823340931cbc41e474cc652826f5`
- **License:** MIT
- **Upstream:** https://github.com/itsloopyo/cameraunlock-core
- **Usage:** Supplies the shared pose pipeline: the UDP receiver, the interpolator,
  the smoothing model, and the camera, lean clamp and crosshair maths the mod is
  built on.
- **Bundled:** yes. Compiled into our `.asi`, and its licence ships under
  `licenses/`.

```
MIT License

Copyright (c) 2026 itsloopyo

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## OpenTrack

- **Version:** n/a. No OpenTrack code is used; the wire format is what we
  interoperate with.
- **License:** ISC
- **Upstream:** https://github.com/opentrack/opentrack
- **Usage:** The mod reads the OpenTrack UDP pose datagram layout, so OpenTrack and
  compatible trackers can drive it.
- **Bundled:** no. Nothing from OpenTrack ships in the release ZIPs or is linked at
  runtime, so its licence triggers no notice obligation here. It is credited because
  the wire format is its work.

---

## Thief

Thief is the property of its publisher and developers. This repository contains no game
code, no extracted assets and no data files, and redistributes none. The mod attaches to a
copy of the game the user already owns, and writes only the files its own installer
places.

What the repository does hold, and where it came from, so the boundary is auditable: the
mod addresses the game by Unreal Engine 3 class and member names (`AController`,
`ACamera`, `AWorldInfo`, `UWorld::SingleLineCheck`, `FCheckResult`) and by numeric offsets
and addresses within the shipped Win64 executable, both recorded in `src/build_profile.cpp`
and in the source files that consume them. Those names are Unreal Engine 3's and those
numbers describe one build of one binary. No engine or game source was copied, and none is
reproduced here.
