// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "config.h"
#include "ue3_math.h"

namespace ThiefHeadTracking {

// Binds the lean clamp to the world trace and takes the configured margin and release
// pacing. Reports what it did to the log. Switched off in the config, or on with no trace
// bound for this build, ClampLean returns before it reaches the policy at all and every
// lean passes through untouched.
void InitCameraCollision(const Config& cfg);

// Cuts the lean (@p dx, @p dy, @p dz, world units, from @p eye) back to whatever the
// level leaves room for, writing the result back over the three components.
//
// Call once per rendered frame - the release pacing integrates real time between calls.
void ClampLean(const UE3Vector& eye, float* dx, float* dy, float* dz);

// Forgets the current allowance, for a frame that applies no lean at all: a closed gate,
// a tracker that stopped sending, rotation-only mode, a camera cut. The next lean takes
// its trace answer outright instead of easing up to it.
void ResetCameraCollision();

}  // namespace ThiefHeadTracking
