// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "ue3_math.h"

namespace ThiefHeadTracking {

// How far down the clean aim the trace looks for something to mark, in world units (cm).
constexpr float kAimTraceRange = 20000.0f;

// The clean aim ray's contact, this frame.
struct AimPoint {
    // False when the trace could not run at all. The reticle is then not placed and the
    // failure is logged, rather than a magic distance being substituted.
    bool  queried = false;
    // True when a surface was found. False with queried=true is a definite no-hit: a
    // target at infinity, which the caller projects as a direction.
    bool  hit = false;
    UE3Vector point{};
    float distance = 0.0f;
};

// Query from the unmodified viewpoint, excluding the controlled pawn.
AimPoint TraceAim(const UE3Vector& eye, const UE3Rotator& cleanRot, const void* sourceActor);

}  // namespace ThiefHeadTracking
