// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include <atomic>

namespace ThiefHeadTracking {

// Where the shot lands, in the head-tracked view the player is looking through.
//
// The camera hook injects head rotation and lean into the scene view only, so the game
// keeps aiming along the rotation the mouse chose from the eye the game put the camera
// at, while the player looks and leans somewhere else. The reticle therefore has to leave
// screen centre.
//
// The hook publishes the vector from the RENDER eye to the impact point, resolved in the
// tracked view's own right/up/forward basis. That is a POINT, not a direction, and the
// difference is the whole of the 6DOF story: a lean separates the rendered eye from the
// eye the shot leaves, and from there a direction-based marker slides off the thing it
// marks, worse the closer the target. The HUD hook turns the vector into a screen
// position with the viewport it is drawing into.
//
// Written on the game thread inside the scene-view hook and read on the game thread in
// the HUD update. Atomic because the two are not guaranteed to be the same thread, and
// deliberately not updated as one transaction: a torn read costs one frame of a few
// pixels on a marker that is already moving.
struct AimMarker {
    // Render eye to impact point, in the tracked view basis: right, up, forward.
    std::atomic<float> right{0.0f};
    std::atomic<float> up{0.0f};
    std::atomic<float> forward{1.0f};
    // Horizontal field of view the scene view is rendered with, in degrees. This is the
    // value the projection matrix was built from, so it carries the game's own zooms.
    //
    // Zero until the hook has published one. A reader that projects from it before then
    // fails the usable-range check and refuses to place the crosshair, which is what a
    // plausible-looking default would quietly stop it doing.
    std::atomic<float> fov_deg{0.0f};
    // The camera's ConstrainedAspectRatio when it is constraining the view's aspect, 0
    // when it is not. UE3 then builds the projection from that ratio rather than the
    // viewport's pixel size and centres the view inside the viewport with bars, so both
    // the vertical scale and the pixel the aim point lands on come from the smaller rect.
    std::atomic<float> constrained_aspect{0.0f};
    // Diagnostics, published so a drift report can be settled by arithmetic on one log
    // line rather than by guessing which term is wrong. The distance is what the aim
    // trace measured to the surface being marked, 0 for a no-hit; the lean is what the
    // hook actually applied this frame, in world units, along the same basis.
    std::atomic<float> distance{0.0f};
    std::atomic<float> lean_right{0.0f};
    std::atomic<float> lean_up{0.0f};
    std::atomic<float> lean_forward{0.0f};
    // False when the aim trace found no surface, so the vector above is the clean aim
    // direction at infinity rather than a point.
    std::atomic<bool> has_point{false};
    // False when tracking is not being injected this frame, so the HUD hook leaves the
    // reticle where the game put it.
    std::atomic<bool> active{false};
};

AimMarker& GetAimMarker();

// One reader's view of the marker, with every field loaded exactly once.
//
// The consumer has to project, write and then report the SAME numbers: reading an atomic
// twice can hand the diagnostic line a different value from the one that placed the
// reticle, which is how a fix gets shipped against a fault that was never there.
struct AimMarkerSample {
    float right, up, forward;
    float fov_deg;
    float constrained_aspect;
    float distance;
    float lean_right, lean_up, lean_forward;
    bool has_point;
    // False when nothing was published for this frame. The publish failure paths and the
    // stand-down clear this flag ALONE and leave every field above holding the last frame
    // that did publish, so a reader that ignores it cannot tell "no aim point this frame"
    // from "an aim point dead ahead" - and neither can the log line that reports it.
    bool active;
};

inline AimMarkerSample SampleAimMarker(const AimMarker& marker) {
    AimMarkerSample s;
    s.right = marker.right.load(std::memory_order_relaxed);
    s.up = marker.up.load(std::memory_order_relaxed);
    s.forward = marker.forward.load(std::memory_order_relaxed);
    s.fov_deg = marker.fov_deg.load(std::memory_order_relaxed);
    s.constrained_aspect = marker.constrained_aspect.load(std::memory_order_relaxed);
    s.distance = marker.distance.load(std::memory_order_relaxed);
    s.lean_right = marker.lean_right.load(std::memory_order_relaxed);
    s.lean_up = marker.lean_up.load(std::memory_order_relaxed);
    s.lean_forward = marker.lean_forward.load(std::memory_order_relaxed);
    s.has_point = marker.has_point.load(std::memory_order_relaxed);
    s.active = marker.active.load(std::memory_order_relaxed);
    return s;
}

}  // namespace ThiefHeadTracking
