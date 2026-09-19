// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "world_trace.h"
#include "logging.h"
#include "memory_probe.h"

#include <cmath>
#include <cstring>

namespace ThiefHeadTracking {
namespace {

// AActor::execTrace calls this wrapper. Its zero return means a blocking hit;
// FCheckResult stores Location at +0x10 and Normal at +0x1C.
using Trace_t = std::int32_t(__fastcall*)(void*, void*, const void*, const float*,
                                         const float*, std::uint32_t, std::uint8_t,
                                         const float*, void*, void*);
Trace_t g_trace = nullptr;
std::uintptr_t g_gworld = 0;
std::uint32_t g_worldFlags = 0;

}  // namespace

void InitWorldTrace(const BuildProfile& profile, std::uintptr_t moduleBase,
                    std::uint32_t channelOverride) {
    if (profile.rvaWorldTrace == 0 || profile.rvaGWorld == 0) {
        Log::Line("World trace unavailable for this build");
        return;
    }
    g_gworld = moduleBase + profile.rvaGWorld;
    g_worldFlags = channelOverride ? channelOverride : profile.traceWorldFlags;
    g_trace = reinterpret_cast<Trace_t>(moduleBase + profile.rvaWorldTrace);
    Log::Line("World trace bound @ %p, flags 0x%X", reinterpret_cast<void*>(g_trace),
              g_worldFlags);
}

bool WorldTraceReady() { return g_trace != nullptr; }

TraceHit TraceWorld(const float start[3], const float dir[3], float maxDistance,
                    const void* sourceActor, std::uint32_t traceFlags) {
    TraceHit out;
    if (!g_trace || !std::isfinite(maxDistance) || maxDistance <= 0.0f ||
        !Readable(g_gworld, sizeof(std::uintptr_t))) {
        return out;
    }
    if (sourceActor && !Readable(reinterpret_cast<std::uintptr_t>(sourceActor),
                                 sizeof(std::uintptr_t))) return out;
    const auto world = *reinterpret_cast<const std::uintptr_t*>(g_gworld);
    if (!Readable(world, sizeof(std::uintptr_t))) return out;
    const float end[3] = {start[0] + dir[0] * maxDistance,
                          start[1] + dir[1] * maxDistance,
                          start[2] + dir[2] * maxDistance};
    const float extent[3] = {};
    alignas(16) std::uint8_t hit[0x80] = {};
    const auto clear = g_trace(reinterpret_cast<void*>(world), hit, sourceActor, end,
                               start, traceFlags ? traceFlags : g_worldFlags, 0x10,
                               extent, nullptr, nullptr);
    out.queried = true;
    if (clear) return out;
    std::memcpy(out.point, hit + 0x10, sizeof(out.point));
    std::memcpy(out.normal, hit + 0x1C, sizeof(out.normal));
    out.distance = (out.point[0] - start[0]) * dir[0] +
                   (out.point[1] - start[1]) * dir[1] +
                   (out.point[2] - start[2]) * dir[2];
    if (!std::isfinite(out.distance) || out.distance < 0.0f || out.distance > maxDistance) {
        Log::Line("ERROR: world trace returned an invalid contact distance %.3f", out.distance);
        out.queried = false;
        return out;
    }
    out.blocked = true;
    return out;
}

}  // namespace ThiefHeadTracking
