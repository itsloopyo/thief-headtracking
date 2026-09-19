// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "game_state.h"
#include <array>
#include <cstdio>
#include <cstring>

namespace {
const void* g_target = nullptr;
const void* __fastcall ViewTarget(const void*) { return g_target; }
}

int main() {
    using namespace ThiefHeadTracking;
    alignas(8) std::array<std::uint8_t, 64> controller{}, pawn{}, camera{}, world{}, level{}, info{};
    std::uintptr_t table[2] = {0, reinterpret_cast<std::uintptr_t>(&ViewTarget)};
    const auto tablePointer = table;
    std::memcpy(controller.data(), &tablePointer, sizeof(tablePointer));
    const auto infoPointer = info.data();
    const auto actors = &infoPointer;
    const auto levelPointer = level.data();
    const auto worldPointer = world.data();
    std::memcpy(world.data() + 8, &levelPointer, sizeof(levelPointer));
    std::memcpy(level.data() + 8, &actors, sizeof(actors));
    BuildProfile profile{};
    profile.offControllerPawn = 0x14;
    profile.vtGetViewTarget = 8;
    profile.rvaGWorld = reinterpret_cast<std::uintptr_t>(&worldPointer);
    profile.offWorldPersistentLevel = 8;
    profile.offLevelActors = 8;
    profile.offWorldInfoPauser = 0x18;
    InitGameState(profile, 0);
    int failures = 0;
    const auto check = [&](const void* controlled, const void* target, const void* pauser,
                           std::uint32_t expected, const char* name) {
        std::memcpy(controller.data() + profile.offControllerPawn, &controlled, sizeof(controlled));
        std::memcpy(info.data() + profile.offWorldInfoPauser, &pauser, sizeof(pauser));
        g_target = target;
        bool aiming = true;
        const auto actual = ReadGameStateGate(controller.data(), &aiming);
        if (actual != expected || aiming) {
            std::printf("FAIL: %s gate=%u expected=%u aiming=%d\n", name, actual, expected, aiming);
            ++failures;
        }
    };
    check(nullptr, controller.data(), nullptr, kGateNoPawn, "front end");
    check(pawn.data(), pawn.data(), nullptr, 0, "gameplay");
    check(pawn.data(), camera.data(), nullptr, 0, "cutscene with possessed pawn");
    check(nullptr, camera.data(), nullptr, kGateNoPawn, "front end menu camera");
    check(pawn.data(), camera.data(), controller.data(), kGatePaused, "paused cutscene");
    check(pawn.data(), pawn.data(), controller.data(), kGatePaused, "pause menu");
    check(nullptr, nullptr, nullptr, kGateNoPawn, "loading without view target");
    return failures ? 1 : 0;
}
