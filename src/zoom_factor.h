// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "fov_range.h"

#include "cameraunlock/camera/zoom_compensation.h"
#include "cameraunlock/math/angle_utils.h"

#include <cmath>

namespace ThiefHeadTracking {

// How much of the head to apply, given the field of view the frame is being rendered at
// and the one this camera renders at unzoomed. Exactly 1 in ordinary play.
//
// A field of view WIDER than the base is passed through unscaled rather than scaled up.
// The two ways Thief renders wider than its own DefaultFOV are the player's field of view
// slider and a display wider than the aspect the projection was authored for, and neither
// is a zoom: both put more of the world on more glass, so a head turn still sweeps the
// same distance across the screen. Multiplying it up would over-drive the view for exactly
// the players who asked for a wide field of view.
//
// Both arguments are HORIZONTAL degrees. Pairing a vertical FOV with a horizontal base is
// the failure this whole conversion is prone to and nothing downstream can catch it: the
// factor is merely off by a constant, so the whole of normal play runs at a fixed fraction
// of the head. Kept here, out of the detour, so the arithmetic can be exercised without a
// running game.
inline float ZoomFactor(float fovDegrees, float baseFovDegrees) {
    if (!IsUsableFov(fovDegrees) || !IsUsableFov(baseFovDegrees) ||
        fovDegrees >= baseFovDegrees) {
        return 1.0f;
    }
    constexpr float kHalfDegToRad = static_cast<float>(cameraunlock::math::kPi / 360.0);
    return cameraunlock::camera::FovZoomFactor(std::tan(fovDegrees * kHalfDegToRad),
                                               std::tan(baseFovDegrees * kHalfDegToRad));
}

}  // namespace ThiefHeadTracking
