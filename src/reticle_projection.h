// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include <cmath>

namespace ThiefHeadTracking {

struct ReticleStage {
    float left, top, right, bottom;
};

inline bool ProjectReticleToStage(const ReticleStage& stage, float projectionX,
                                  float projectionY, float right, float up, float forward,
                                  float& x, float& y) {
    if (!std::isfinite(forward) || forward <= 0.0f ||
        !std::isfinite(projectionX) || projectionX <= 0.0f ||
        !std::isfinite(projectionY) || projectionY <= 0.0f) return false;
    const float nx = projectionX * right / forward;
    const float ny = projectionY * up / forward;
    if (!std::isfinite(nx) || !std::isfinite(ny) ||
        std::fabs(nx) > 1.0f || std::fabs(ny) > 1.0f) return false;
    x = stage.left + (nx + 1.0f) * 0.5f * (stage.right - stage.left);
    y = stage.top + (1.0f - ny) * 0.5f * (stage.bottom - stage.top);
    return std::isfinite(x) && std::isfinite(y);
}

}  // namespace ThiefHeadTracking
