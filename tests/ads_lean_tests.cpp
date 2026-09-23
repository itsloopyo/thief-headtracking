// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo
//
// Head tracking stays on while the bow is drawn. The lean is the only thing the draw eases
// out, and these checks hold the camera hook to that: the hook scales x, y and z by
// LeanEase and hands rotation through untouched, so a pose run through the same two steps
// here is what reaches the camera.

#include "ads.h"
#include "tracking_runtime.h"

#include <cmath>
#include <cstdio>

namespace {

int g_failures = 0;

void Check(bool ok, const char* what) {
    if (!ok) {
        ++g_failures;
        std::printf("FAIL: %s\n", what);
    }
}

void CheckNear(float got, float want, const char* what) {
    if (!(std::fabs(got - want) < 1e-4f)) {
        ++g_failures;
        std::printf("FAIL: %s (got %.5f, want %.5f)\n", what, got, want);
    }
}

using ThiefHeadTracking::FrameSample;
using ThiefHeadTracking::LeanEase;

constexpr unsigned long long kLowerMs = cameraunlock::ads::AdsFade::kLowerMs;
constexpr unsigned long long kRaiseMs = cameraunlock::ads::AdsFade::kRaiseMs;

FrameSample Pose() {
    FrameSample s;
    s.has_rotation = true;
    s.pitch = 10.0f;
    s.yaw = 20.0f;
    s.roll = 7.0f;
    s.has_position = true;
    s.pos_x = 0.1f;
    s.pos_y = 0.2f;
    s.pos_z = -0.3f;
    return s;
}

// The two steps the camera hook runs on a frame.
FrameSample Apply(LeanEase& ease, bool aiming, unsigned long long nowMs) {
    FrameSample s = Pose();
    const float scale = ease.Update(aiming, nowMs);
    s.pos_x *= scale;
    s.pos_y *= scale;
    s.pos_z *= scale;
    return s;
}

void CheckRotationUntouched(const FrameSample& s, const char* when) {
    char what[128];
    std::snprintf(what, sizeof(what), "pitch is absolute and unscaled %s", when);
    CheckNear(s.pitch, 10.0f, what);
    std::snprintf(what, sizeof(what), "yaw is absolute and unscaled %s", when);
    CheckNear(s.yaw, 20.0f, what);
    std::snprintf(what, sizeof(what), "roll is absolute and unscaled %s", when);
    CheckNear(s.roll, 7.0f, what);
}

void HipFirePassesThePoseThrough() {
    LeanEase ease;
    const FrameSample s = Apply(ease, false, 1000);
    CheckRotationUntouched(s, "at the hip");
    CheckNear(s.pos_x, 0.1f, "the lean is kept at the hip on x");
    CheckNear(s.pos_y, 0.2f, "and on y");
    CheckNear(s.pos_z, -0.3f, "and on z");
}

void TheDrawnBowTakesOnlyTheLean() {
    LeanEase ease;
    Apply(ease, true, 1000);
    const FrameSample s = Apply(ease, true, 1000 + kLowerMs);
    CheckRotationUntouched(s, "with the bow drawn");
    CheckNear(s.pos_x, 0.0f, "x goes to zero with the bow drawn");
    CheckNear(s.pos_y, 0.0f, "and y");
    CheckNear(s.pos_z, 0.0f, "and z");
}

// The frame the bow comes up on is still the full lean: the ease leaves at rest.
void TheDrawFrameDoesNotStep() {
    LeanEase ease;
    Apply(ease, false, 1000);
    const FrameSample s = Apply(ease, true, 1016);
    CheckNear(s.pos_x, 0.1f, "the draw frame keeps the full lean");
}

void MidTransitionOnlyTheLeanIsScaled() {
    LeanEase ease;
    Apply(ease, true, 1000);
    const FrameSample s = Apply(ease, true, 1000 + kLowerMs / 2);
    CheckRotationUntouched(s, "half way through the draw");
    Check(s.pos_x > 0.0f && s.pos_x < 0.1f, "the lean is part way out half way through");
    CheckNear(s.pos_y / 0.2f, s.pos_x / 0.1f, "every lean axis rides the same scale");
    CheckNear(s.pos_z / -0.3f, s.pos_x / 0.1f, "z included");
}

// A tap of the aim button: the lean turns round where it is rather than stepping.
void AReversalContinuesFromWhereItWas() {
    LeanEase ease;
    Apply(ease, true, 1000);
    const FrameSample down = Apply(ease, true, 1000 + kLowerMs / 2);
    const FrameSample turned = Apply(ease, false, 1000 + kLowerMs / 2);
    CheckNear(turned.pos_x, down.pos_x, "the release frame starts from where the draw got to");

    const FrameSample next = Apply(ease, false, 1000 + kLowerMs / 2 + 16);
    Check(next.pos_x > turned.pos_x, "and heads back out from there");
    Check(next.pos_x - turned.pos_x < 0.02f, "without a step");

    const FrameSample home = Apply(ease, false, 1000 + kLowerMs / 2 + kRaiseMs);
    CheckNear(home.pos_x, 0.1f, "the whole lean is back once the raise has run");
    CheckRotationUntouched(home, "after the bow is lowered");
}

void ResetReturnsToTheHip() {
    LeanEase ease;
    Apply(ease, true, 1000);
    Apply(ease, true, 1000 + kLowerMs);
    ease.Reset();
    const FrameSample s = Apply(ease, false, 2000);
    CheckNear(s.pos_x, 0.1f, "a reset puts the whole lean back");
}

}  // namespace

int main() {
    HipFirePassesThePoseThrough();
    TheDrawnBowTakesOnlyTheLean();
    TheDrawFrameDoesNotStep();
    MidTransitionOnlyTheLeanIsScaled();
    AReversalContinuesFromWhereItWas();
    ResetReturnsToTheHip();

    if (g_failures != 0) {
        std::printf("%d ADS lean check(s) failed\n", g_failures);
        return 1;
    }
    std::printf("ADS lean tests passed\n");
    return 0;
}
