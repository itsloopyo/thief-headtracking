// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "camera_collision.h"

#include "lean_geometry.h"
#include "logging.h"
#include "world_trace.h"

#include "cameraunlock/camera/lean_clamp.h"

#include <windows.h>

namespace ThiefHeadTracking {

namespace {

// Longest gap the release is allowed to integrate over. A frame that took longer than
// this was a hitch or a load, and pacing the release across it would let the whole
// recovery happen in one step, which is the jump the pacing exists to prevent.
constexpr float kMaxDt = 0.1f;

cameraunlock::camera::LeanClamp g_clamp;
bool g_enabled = false;
float g_margin = 0.0f;
LARGE_INTEGER g_lastTick{};

float ElapsedSeconds() {
    static const double kSecondsPerCount = [] {
        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);
        return 1.0 / static_cast<double>(freq.QuadPart);
    }();

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    const LONGLONG previous = g_lastTick.QuadPart;
    g_lastTick = now;
    if (previous == 0 || now.QuadPart <= previous) {
        return 0.0f;
    }
    const float dt = static_cast<float>(static_cast<double>(now.QuadPart - previous) *
                                        kSecondsPerCount);
    return dt > kMaxDt ? kMaxDt : dt;
}

// The engine half of the clamp. Core owns what to do with the answer, lean_geometry.h owns
// the trace's own arithmetic, and this only asks the engine what is in the way.
// The context is core's callback shape and this query has no use for one: the trace takes
// no source actor.
bool g_queryRan = false;

cameraunlock::camera::LeanObstruction Query(void* context,
                                            const cameraunlock::math::Vec3& start,
                                            const cameraunlock::math::Vec3& direction,
                                            float maxDistance) {
    (void)context;
    g_queryRan = true;
    cameraunlock::camera::LeanObstruction out;

    const float startArr[3] = { start.x, start.y, start.z };
    const float dirArr[3] = { direction.x, direction.y, direction.z };

    const TraceHit hit =
        TraceWorld(startArr, dirArr, LeanTraceLength(maxDistance, g_margin));
    if (!hit.queried) {
        return out;
    }
    out.queried = true;
    if (!hit.blocked) {
        return out;
    }

    // The normal faces back along the lean, so this is positive for a lean running into
    // the surface.
    const float approachCos = -(dirArr[0] * hit.normal[0] + dirArr[1] * hit.normal[1] +
                                dirArr[2] * hit.normal[2]);

    out.blocked = true;
    out.distance = LeanContactDistance(hit.distance, approachCos, g_margin);
    return out;
}

// Every change in what the clamp is doing, and a periodic sample between changes.
//
// Both, because neither alone answers the question a bug report asks. Transitions on their
// own cannot separate "the sweep runs and the room is open" from "the sweep is not
// running", which need different fixes; a first-occurrence-only line is worse still, since
// a player who leans through a wall in the fifth mission sends a log carrying one contact
// line from a doorframe in the first. The transition lines are budgeted because contact
// flips on and off as a head leans at an edge, and a render-rate flip would otherwise fill
// the file; the sample keeps reporting after the budget is spent.
constexpr int kMaxTransitionReports = 20;
constexpr unsigned long long kSampleIntervalMs = 30000ull;

bool g_reportKnown = false;
bool g_reportQueried = false;
bool g_reportContact = false;
bool g_reportFailed = false;
int g_reportTransitions = 0;
unsigned long long g_reportSampleMs = 0;

void ReportState(bool queried, bool contact, bool queryFailed, float requested,
                 float allowed) {
    const unsigned long long now = GetTickCount64();
    const bool changed = !g_reportKnown || queried != g_reportQueried ||
                         contact != g_reportContact || queryFailed != g_reportFailed;
    const bool budgeted = changed && g_reportTransitions < kMaxTransitionReports;
    const bool due = now - g_reportSampleMs >= kSampleIntervalMs;

    if (changed) {
        g_reportKnown = true;
        g_reportQueried = queried;
        g_reportContact = contact;
        g_reportFailed = queryFailed;
        if (budgeted) {
            ++g_reportTransitions;
        }
    }
    if (!budgeted && !due) {
        return;
    }
    g_reportSampleMs = now;

    if (queryFailed) {
        Log::Line("WARN: the lean trace could not run; leaning is unclamped and can put "
                  "the view inside geometry");
        return;
    }
    if (contact) {
        Log::Line("Camera collision: a %.1f unit lean met the world and was held to %.1f",
                  requested, allowed);
        return;
    }
    if (!queried) {
        // The policy skips the query entirely for a lean below its own minimum, so "no
        // contact and no failure" covers two states. Saying the trace is running when it
        // was never called is the confusion this reporting exists to remove.
        Log::Line("Camera collision: armed, with no lean to test (the head is centred)");
        return;
    }
    Log::Line("Camera collision: the trace ran and the lean is clear (%.1f units "
              "requested, %.1f allowed)", requested, allowed);
}

}  // namespace

void InitCameraCollision(const Config& cfg) {
    g_clamp.Reset();
    g_lastTick.QuadPart = 0;
    g_margin = cfg.lean_clamp.skin;
    g_enabled = cfg.collision_enabled && WorldTraceReady();

    // The policy's skin and the trace's margin are the same number on purpose, and
    // LeanTraceLength depends on it: core adds the skin to the lean before it calls the
    // query, and that function subtracts the margin back off to land the ray exactly one
    // overreach past the requested lean. Set them from two different values and the ray is
    // short by the difference, with nothing to say so - the trace reports an honest miss.
    g_clamp.SetSettings(cfg.lean_clamp);

    if (!cfg.collision_enabled) {
        Log::Line("Camera collision off by config: leaning will push the view through "
                  "walls it gets close enough to");
        return;
    }
    if (!g_enabled) {
        Log::Line("WARN: CollisionEnabled is on but the world trace is not bound; leaning "
                  "is unclamped");
        return;
    }
    Log::Line("Camera collision on: the lean is traced against the world and stops %.1f "
              "units short of what it hits", g_margin);
}

void ClampLean(const UE3Vector& eye, float* dx, float* dy, float* dz) {
    if (!g_enabled) {
        return;
    }

    const cameraunlock::math::Vec3 eyeVec{ eye.X, eye.Y, eye.Z };
    const cameraunlock::math::Vec3 desired{ *dx, *dy, *dz };
    g_queryRan = false;
    const cameraunlock::math::Vec3 allowed =
        g_clamp.Apply(eyeVec, desired, ElapsedSeconds(), &Query, nullptr);

    ReportState(g_queryRan, g_clamp.InContact(), g_clamp.LastQueryFailed(),
                desired.Magnitude(), allowed.Magnitude());

    *dx = allowed.x;
    *dy = allowed.y;
    *dz = allowed.z;
}

void ResetCameraCollision() {
    g_clamp.Reset();
    g_lastTick.QuadPart = 0;
}

}  // namespace ThiefHeadTracking
