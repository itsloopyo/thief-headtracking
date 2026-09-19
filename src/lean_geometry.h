// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

namespace ThiefHeadTracking {

// The two adjustments a LINE trace against a surface with a normal has to make before the
// core lean policy sees its answer. Both are properties of the trace, not of the policy,
// which is deliberately unaware of either - and both are pure arithmetic, so they live
// here rather than inside the query that calls the engine.

// The shallowest approach angle the pull-back is divided by.
//
// The stand-off is held along the SURFACE NORMAL, so a lean that meets a wall at a
// glancing angle has to stop further back along its own direction than one that meets it
// head on. That scaling is 1/cos of the approach angle and runs away to infinity as the
// lean turns parallel to the surface, so it is floored here. A hit that glancing is not
// the one about to put the eye through the wall anyway.
constexpr float kMinApproachCos = 0.2f;

// How far the ray is cast for a lean of @p maxDistance held @p margin off what it hits.
//
// It REACHES PAST the lean. A ray that stopped where the lean stops cannot see the surface
// the lean is about to come to rest against, so the eye would travel the whole lean, arrive
// against the wall, and only pop back out once the head pushed far enough for the ray
// itself to cross the surface. The overreach is the margin at the shallowest approach the
// pull-back is computed for.
//
// @p maxDistance is what the core policy hands its query, which is the lean the head asked
// for PLUS the policy's own skin - core adds it before calling. The `- margin` below is
// what cancels that pre-addition, so the ray ends up exactly one overreach past the
// requested lean. It only cancels while the skin and @p margin are the same number, which
// is why InitCameraCollision sets both from the one config value and says so there.
inline float LeanTraceLength(float maxDistance, float margin) {
    const float overreach = margin / kMinApproachCos;
    return maxDistance - margin + overreach;
}

// The contact distance handed to the policy: @p hitDistance pulled in by the extra the
// approach angle costs, so the policy's flat subtraction of the skin lands the eye a full
// @p margin off the surface measured along that surface's normal.
//
// @p approachCos is -(direction . normal), so it is positive for a lean running into the
// surface. A degenerate normal reads as a glancing hit and takes the floored pull-back,
// which stops the lean short - the safe direction to fail. Never negative: a pull-back
// deeper than the hit itself means the eye may not move at all.
//
// The cosine is bounded at BOTH ends, and the ceiling is the one that matters. A cosine
// cannot exceed 1, but the normal it is computed from is read at a struct offset inferred
// from Location's rather than confirmed, so it is not guaranteed to be a unit vector.
// Above 1 the pull-back goes NEGATIVE and this hands back a contact further away than the
// trace found it; the policy then subtracts its skin from an inflated distance, the two
// cancel, and the eye is allowed all the way onto the surface - inside the near plane,
// which is the see-through-the-wall failure the stand-off exists to prevent. A non-finite
// cosine fails the floor comparison and takes the floor, which is the safe direction.
inline float LeanContactDistance(float hitDistance, float approachCos, float margin) {
    float cosine = approachCos > kMinApproachCos ? approachCos : kMinApproachCos;
    if (cosine > 1.0f) {
        cosine = 1.0f;
    }
    const float extra = margin / cosine - margin;
    const float distance = hitDistance - extra;
    return distance < 0.0f ? 0.0f : distance;
}

}  // namespace ThiefHeadTracking
