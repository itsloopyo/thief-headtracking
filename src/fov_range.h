// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include <cmath>

namespace ThiefHeadTracking {

// The range a field of view stays usable over. Outside it the projection is not
// something the player can see through - at 0 it degenerates and the screen goes
// black - and a value this far out read off the camera is a mis-read rather than a
// choice.
//
// One definition for the three places that need it: the camera gate refuses to inject
// when the camera reports a FOV outside it, the zoom compensation refuses to scale from
// one, and the aim projection refuses to place a crosshair from one.
constexpr float kMinFovDegrees = 20.0f;
constexpr float kMaxFovDegrees = 170.0f;

inline bool IsUsableFov(float fovDegrees) {
    return std::isfinite(fovDegrees) &&
           fovDegrees >= kMinFovDegrees && fovDegrees <= kMaxFovDegrees;
}

}  // namespace ThiefHeadTracking
