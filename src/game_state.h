// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "build_profile.h"

#include <cstdint>

namespace ThiefHeadTracking {

// Why the frame is not one to inject into. Reported as bits rather than a bool so the
// log says which state closed the gate, and a state that never closes is visible by its
// absence.
enum GateBit : std::uint32_t {
    kGateNoCamera  = 1u << 0,  // the controller has no PlayerCamera yet
    kGateBadPov    = 1u << 1,  // the viewpoint or its field of view did not read sanely
    kGateTransition = 1u << 2, // the engine is loading, saving or paused
    kGatePaused    = 1u << 3,  // the world has a pauser: pause menu, inventory, map
    kGateNoPawn    = 1u << 6,  // the controller possesses nothing: front end menu, loading
};

// Binds the globals and offsets the gate reads. Every one of them is validated on every
// read rather than cached, because a level load rebuilds the objects behind them.
void InitGameState(const BuildProfile& profile, std::uintptr_t moduleBase);

// The gate bits for this frame, given the controller the scene view asked the viewpoint
// of. Zero allows gameplay and scripted cameras.
//
// Fails OPEN: a pointer walk that cannot complete gates nothing. A wrong read here that
// failed closed would switch head tracking off for the whole session with nothing in the
// log to say why, which is a far worse failure than tracking briefly running in a menu.
//
// @p outAiming reports whether the bow is drawn. It is set on every path, including the
// ones that return early, so nothing downstream can read a stale flag. It never closes the
// gate: the camera hook only uses it to ease the lean out while the bow is drawn.
std::uint32_t ReadGameStateGate(const std::uint8_t* controller, bool* outAiming);

}  // namespace ThiefHeadTracking
