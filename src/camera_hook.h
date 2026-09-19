// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "build_profile.h"
#include "config.h"
#include "tracking_runtime.h"

#include <cstdint>

namespace ThiefHeadTracking {

// Installs the two detours that between them decouple the view from the aim:
// ULocalPlayer::CalcSceneView, which says whether the frame being built is the one the
// player looks through, and AController::eventGetPlayerViewPoint, which is where the head
// pose is added.
// @p reticleAvailable says whether this build carries a consumer for the aim marker. With
// none, the marker is not published: producing one costs a world trace on every rendered
// frame and nothing would read it.
bool InstallCameraHook(const BuildProfile& profile, std::uintptr_t moduleBase,
                       TrackingRuntime& tracking, const Config& cfg, bool reticleInstalled);

}  // namespace ThiefHeadTracking
