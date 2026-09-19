// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "reticle_projection.h"
#include "ue3_math.h"
#include <cmath>
#include <cstdio>
#include <limits>

int main() {
    using namespace ThiefHeadTracking;
    int failures = 0;
    const auto check = [&](bool ok, const char* name) {
        if (!ok) { std::printf("FAIL: %s\n", name); ++failures; }
    };
    const ReticleStage stage{0, 0, 1280, 720};
    float x = 0, y = 0;
    check(ProjectReticleToStage(stage, 1, 16.0f / 9, 0, 0, 10, x, y) &&
          x == 640 && y == 360, "centred aim");
    const UE3Vector point{100, 0, 0};
    const UE3Vector eye{0, 10, 0};
    float ruf[3];
    ResolvePointInTrackedView(eye, point, UE3Rotator{0, 0, 0}, ruf);
    check(ProjectReticleToStage(stage, 1, 16.0f / 9, ruf[0], ruf[1], ruf[2], x, y) &&
          std::fabs(x - 576) < 0.01f, "lean preserves a near world target");
    ResolvePointInTrackedView(UE3Vector{}, point, UE3Rotator{0, DegToUnits(45), 0}, ruf);
    check(ProjectReticleToStage(stage, 1, 16.0f / 9, ruf[0], ruf[1], ruf[2], x, y) &&
          std::fabs(x) < 0.01f, "head yaw leaves the target at the view edge");
    check(ProjectReticleToStage(ReticleStage{-640, 0, 1920, 720}, 1, 32.0f / 9,
                                0.5f, 0.1f, 1, x, y) &&
          std::fabs(x - 1280) < 0.01f && std::fabs(y - 232) < 0.01f,
          "wide viewport uses its actual projection and movie bounds");
    check(!ProjectReticleToStage(stage, 1, 1, 2, 0, 1, x, y), "off-screen aim is not clamped");
    check(!ProjectReticleToStage(stage, 1, 1, 0, 0, -1, x, y), "aim behind the camera is hidden");
    check(!ProjectReticleToStage(stage, 1, 1, 0, 0, 0, x, y), "no perspective divide at zero depth");
    check(!ProjectReticleToStage(stage, 1, 1, std::numeric_limits<float>::quiet_NaN(), 0, 1, x, y),
          "invalid aim is hidden");
    return failures ? 1 : 0;
}
