// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "build_profile.h"

#include <cstdint>

namespace ThiefHeadTracking {

// One line trace against the level, in world units (cm).
struct TraceHit {
    // False when the trace could not be performed at all - the binding is not installed,
    // or GWorld does not currently hold a usable pointer. Distinct from a definite miss,
    // because a clamp that has quietly stopped clamping looks exactly like one that never
    // engaged.
    bool  queried = false;
    // True when something along the ray blocks it.
    bool  blocked = false;
    // Distance from the start along the ray, and the contact itself.
    float distance = 0.0f;
    float point[3] = { 0.0f, 0.0f, 0.0f };
    float normal[3] = { 0.0f, 0.0f, 0.0f };
};

void InitWorldTrace(const BuildProfile& profile, std::uintptr_t moduleBase,
                    std::uint32_t channelOverride);
bool WorldTraceReady();

TraceHit TraceWorld(const float start[3], const float dir[3], float maxDistance,
                    const void* sourceActor = nullptr, std::uint32_t traceFlags = 0);

}  // namespace ThiefHeadTracking