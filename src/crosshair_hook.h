// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "build_profile.h"

namespace ThiefHeadTracking {

void InitCrosshair(const BuildProfile& profile, std::uintptr_t moduleBase);
void UpdateCrosshair(const void* sceneView, bool trackingInjected);

}  // namespace ThiefHeadTracking
