// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "tracking_runtime.h"
#include "ue3_math.h"

#include <cstdint>

namespace ThiefHeadTracking {

// The camera hook's log, kept apart from the injection it reports on.
//
// Every one of these fires from a detour on the render path, so each carries its own cap
// or rate limit and its own once-per-session state. Holding them here rather than beside
// the injection is what keeps the hook itself readable as camera code: the file that
// decides what the player sees no longer also owns six counters that only decide what the
// log says.
//
// Game thread only, like the hook. None of the counters is synchronised, for the same
// reason TrackingRuntime::SampleFrame needs none.

// Every term of the zoom factor, once, on the first frame the CAMERA reports a field of
// view rather than the first frame a pose arrives. A factor that is wrong by a constant
// reads exactly like a factor that is right - the whole of normal play simply runs at a
// fixed fraction of the head, and head tracking feels weak everywhere instead of wrong
// anywhere - so the terms have to be somewhere a human can check them, and taking them off
// the camera puts them there with no tracker connected.
//
// Both numbers are HORIZONTAL degrees, so there is no aspect conversion standing between
// them to get backwards: CalcSceneView builds its projection as 1/tan(FOV/2) on x from the
// value GetFOVAngle returns, and the LOD line beside it divides that same call by the
// camera's DefaultFOV.
//
// The gate is that this reads 1.0000 while nothing is zooming.
void LogZoomBasis(float fovDegrees, float baseFovDegrees);

// Why injection stopped, one line per transition rather than one per frame.
//
// The verdict is decided in two steps - the game state, then the sights - and only the
// final one is worth a line, which is why the de-duplication lives here rather than at the
// call sites. Capped as well as de-duplicated: the pointer walks behind the game-state
// bits read structures that are being rebuilt while a level streams in, so the gate can
// flap frame to frame and one bad load would otherwise write a line per frame for as long
// as it lasted.
void ReportGate(std::uint32_t gateBits, float fovDegrees);

// "No head tracking in game" has exactly two causes worth distinguishing: the hook never
// runs, or it runs but never sees the scene-view caller. Reports the call counts every few
// seconds until an injection actually happens, then goes quiet for the session.
void LogTraffic(bool fromSceneView, bool injected);

void ReportViewTarget(const void* pawn, const void* viewTarget);

// Names each distinct caller of CalcSceneView once, by RVA, and says whether it is being
// head tracked. @p moduleBase is what the return address is reported relative to.
void LogSceneViewCaller(std::uintptr_t caller, std::uintptr_t moduleBase, bool isDraw);

// Every term the injected frame depends on, on ONE line, for a bounded burst.
//
// Reading the pose from one line and the lean from another is how a fix gets shipped
// against the wrong fault: each half-reading fits several of them equally well. So the
// tracker pose, the rotator before and after, the lean the tracker asked for, the lean the
// world left room for, and the aim point the reticle would be placed from all go out
// together, on the frame they belong to.
//
// Capped and rate limited because it fires from the render path. It is the line a session
// checks the axis signs and the clamp against, not a running commentary.
void ReportGeometry(const FrameSample& sample, const UE3Rotator& clean,
                    const UE3Rotator& tracked, const UE3Vector& cleanEye,
                    const UE3Vector& renderEye, const float leanRuf[3], float fovDegrees,
                    float zoom);

}  // namespace ThiefHeadTracking
