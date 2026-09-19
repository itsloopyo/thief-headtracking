// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "camera_diagnostics.h"

#include "aim_marker.h"
#include "game_state.h"
#include "logging.h"
#include "zoom_factor.h"

#include <windows.h>

#include <atomic>

namespace ThiefHeadTracking {

namespace {

constexpr DWORD kTrafficReportIntervalMs = 5000;
// Ceilings on the two diagnostics that fire from a per-frame detour. Both describe a
// state that does not change on its own, so repeating them for a whole session adds
// nothing a reader did not have after the first few lines.
constexpr int kMaxTrafficReports = 6;
constexpr int kMaxGateReports = 64;

// Distinct CalcSceneView callers worth naming. The shipped build has three; the spare
// room is for a patch that adds one.
constexpr int kMaxLoggedCallers = 6;

// Rate and ceiling on the per-frame geometry burst.
constexpr DWORD kGeometryIntervalMs = 1000;
constexpr int kMaxGeometryLines = 20;

// Camera frames the zoom basis is allowed to read badly before it is called a fault. A
// camera the engine has only just constructed answers one side of the pair out of range
// for a frame or two, and this function has exactly one line to spend on the session.
constexpr int kZoomBasisGraceFrames = 120;

void LogGateChange(std::uint32_t bits, float fovDegrees) {
    static int s_reports = 0;
    if (s_reports >= kMaxGateReports) {
        return;
    }
    ++s_reports;
    Log::Line("Gate 0x%02X%s%s%s%s%s%s fov=%.1f%s", bits,
              (bits & kGateNoCamera)   ? " NOCAMERA" : "",
              (bits & kGateBadPov)     ? " BADPOV" : "",
              (bits & kGateTransition) ? " TRANSITION" : "",
              (bits & kGatePaused)     ? " PAUSED" : "",
              (bits & kGateNoPawn)     ? " NOPAWN" : "",
              (bits & kGateAds)        ? " ADS" : "",
              fovDegrees,
              s_reports == kMaxGateReports ? " (further gate changes not logged)" : "");
}

}  // namespace

void LogZoomBasis(float fovDegrees, float baseFovDegrees) {
    static bool s_logged = false;
    static int s_unreadableFrames = 0;
    if (s_logged) {
        return;
    }
    const bool fovOk = IsUsableFov(fovDegrees);
    const bool baseOk = IsUsableFov(baseFovDegrees);

    if (!fovOk || !baseOk) {
        // Not yet, rather than not at all. A camera the engine has only just constructed
        // reads one side badly for a frame or two - UObject memory is zeroed, so DefaultFOV
        // answers 0 while GetFOVAngle is already answering 90 - and this function has a
        // single line to spend. Spending it on that transient would leave the log asserting
        // compensation is off for a session that then runs at a correct 1.0000 the whole
        // way, which is worse than saying nothing: it is the units gate reporting a fault
        // that is not there.
        if (++s_unreadableFrames < kZoomBasisGraceFrames) {
            return;
        }
        s_logged = true;
        // Still unreadable after a run of camera frames, so it is not settling. This is the
        // one thing that must never be silent: a factor that is wrong reads exactly like a
        // factor that is right, and without this line the only symptom is "tracking goes
        // wild when the game zooms".
        Log::Line("WARN: zoom compensation is off for this session - after %d camera frames "
                  "the rendered field of view reads %.2f and the unzoomed one %.2f, and "
                  "both have to be between %.0f and %.0f degrees. Head tracking will feel "
                  "exaggerated wherever the game narrows the view.",
                  kZoomBasisGraceFrames, fovDegrees, baseFovDegrees,
                  kMinFovDegrees, kMaxFovDegrees);
        return;
    }
    s_logged = true;

    // The UNCLAMPED ratio as well as the factor, because the factor alone cannot fail the
    // units gate in one direction. ZoomFactor passes a view wider than its base straight
    // through at 1.0, so a base misread too SMALL - a vertical number paired with a
    // horizontal rendered one is the classic - reads 1.0000 here and looks perfect, while
    // every real zoom afterwards is computed against the wrong base. The raw ratio does not
    // hide it: paired correctly it is 1.0000 in ordinary play, and anything else is the
    // pairing rather than the game zooming.
    constexpr float kHalfDegToRad = static_cast<float>(cameraunlock::math::kPi / 360.0);
    const float ratio = std::tan(fovDegrees * kHalfDegToRad) /
                        std::tan(baseFovDegrees * kHalfDegToRad);
    Log::Line("Zoom compensation: rendered FOV %.2f deg horizontal, unzoomed %.2f deg "
              "horizontal, factor %.4f (unclamped ratio %.4f). Read on the first frame the "
              "camera reports, so anything but 1.0000 means the base is wrong rather than "
              "that the game is zooming.", fovDegrees, baseFovDegrees,
              ZoomFactor(fovDegrees, baseFovDegrees), ratio);
}

void ReportGate(std::uint32_t gateBits, float fovDegrees) {
    static std::uint32_t s_lastGate = 0xFFFFFFFFu;
    if (gateBits == s_lastGate) {
        return;
    }
    s_lastGate = gateBits;
    LogGateChange(gateBits, fovDegrees);
}

// The one diagnostic here that runs on the multi-threaded side of the viewpoint detour.
// The scene view is built on the game thread, but the accessor it hooks is the shared one
// - weapon aim, interaction traces, AI and audio all reach the viewpoint through it, from
// whichever thread wanted it, which is why the detour tracks its scene-view depth in
// thread-local storage. This counts those calls, so every static below is atomic: plain
// ones were a data race on the counters and let two threads write the same report.
void LogTraffic(bool fromSceneView, bool injected) {
    static std::atomic<bool> s_confirmed{false};
    static std::atomic<DWORD> s_lastLog{0};
    static std::atomic<unsigned> s_total{0};
    static std::atomic<unsigned> s_scene{0};
    static std::atomic<int> s_reports{0};

    constexpr auto relaxed = std::memory_order_relaxed;

    if (s_confirmed.load(relaxed)) {
        return;
    }
    // Ahead of the report cap: the confirmation is the line the whole diagnostic exists
    // to reach, and injection can start long after the reports have run out.
    if (injected) {
        if (!s_confirmed.exchange(true, relaxed)) {
            Log::Line("Scene-view injection confirmed: head tracking is reaching the "
                      "rendered view and nothing else");
        }
        return;
    }
    if (s_reports.load(relaxed) >= kMaxTrafficReports) {
        return;
    }

    s_total.fetch_add(1, relaxed);
    if (fromSceneView) s_scene.fetch_add(1, relaxed);
    const DWORD now = GetTickCount();
    DWORD last = s_lastLog.load(relaxed);
    if (last == 0) {
        s_lastLog.compare_exchange_strong(last, now, relaxed);
        return;
    }
    if (now - last < kTrafficReportIntervalMs) {
        return;
    }
    // Claiming the interval is what elects the reporter. A thread that loses the exchange
    // has already counted its own call and simply carries on, so the report is written
    // once and the counts it drains are whole.
    if (!s_lastLog.compare_exchange_strong(last, now, relaxed)) {
        return;
    }
    const int reports = s_reports.fetch_add(1, relaxed) + 1;
    const unsigned total = s_total.exchange(0, relaxed);
    const unsigned scene = s_scene.exchange(0, relaxed);
    Log::Line("Viewpoint: %u calls in the last %us, %u of them from the scene view, none "
              "injected%s", total,
              static_cast<unsigned>(kTrafficReportIntervalMs / 1000), scene,
              reports == kMaxTrafficReports
                  ? "; no further viewpoint reports this session" : "");
}

void ReportViewTarget(const void* pawn, const void* viewTarget) {
    static const void* s_pawn = nullptr;
    static const void* s_target = nullptr;
    if (pawn == s_pawn && viewTarget == s_target) {
        return;
    }
    s_pawn = pawn;
    s_target = viewTarget;
    Log::Line("View target: pawn=%p target=%p; cutscene tracking enabled", pawn, viewTarget);
}

void LogSceneViewCaller(std::uintptr_t caller, std::uintptr_t moduleBase, bool isDraw) {
    static std::uintptr_t s_seen[kMaxLoggedCallers] = {};
    static int s_count = 0;
    for (int i = 0; i < s_count; ++i) {
        if (s_seen[i] == caller) return;
    }
    // Full table: a caller that cannot be remembered must not be logged either, or every
    // frame it appears on writes the same line again.
    if (s_count == kMaxLoggedCallers) {
        return;
    }
    s_seen[s_count++] = caller;
    Log::Line("Scene view requested by RVA 0x%06X - %s",
              static_cast<unsigned>(caller - moduleBase),
              isDraw ? "head tracked" : "left alone");
}

void ReportGeometry(const FrameSample& sample, const UE3Rotator& clean,
                    const UE3Rotator& tracked, const UE3Vector& cleanEye,
                    const UE3Vector& renderEye, const float leanRuf[3], float fovDegrees,
                    float zoom) {
    static DWORD s_last = 0;
    static int s_lines = 0;
    if (s_lines >= kMaxGeometryLines) {
        return;
    }
    const DWORD now = GetTickCount();
    if (s_last != 0 && now - s_last < kGeometryIntervalMs) {
        return;
    }
    s_last = now;
    ++s_lines;

    const AimMarkerSample m = SampleAimMarker(GetAimMarker());
    Log::Line("GEO head(y,p,r)=(%.2f,%.2f,%.2f) pos(x,y,z)m=(%.3f,%.3f,%.3f) "
              "clean(p,y,r)deg=(%.2f,%.2f,%.2f) tracked(p,y,r)deg=(%.2f,%.2f,%.2f) "
              "eye=(%.1f,%.1f,%.1f)->(%.1f,%.1f,%.1f) lean(r,u,f)cm=(%.1f,%.1f,%.1f) "
              "fov=%.2f zoom=%.4f aim(r,u,f)=(%.3f,%.3f,%.3f) dist=%.1f hit=%d live=%d%s",
              sample.yaw, sample.pitch, sample.roll,
              sample.pos_x, sample.pos_y, sample.pos_z,
              UnitsToDegrees(clean.Pitch), UnitsToDegrees(clean.Yaw),
              UnitsToDegrees(clean.Roll), UnitsToDegrees(tracked.Pitch),
              UnitsToDegrees(tracked.Yaw), UnitsToDegrees(tracked.Roll),
              cleanEye.X, cleanEye.Y, cleanEye.Z, renderEye.X, renderEye.Y, renderEye.Z,
              leanRuf[0], leanRuf[1], leanRuf[2], fovDegrees, zoom,
              m.right, m.up, m.forward, m.distance, m.has_point ? 1 : 0, m.active ? 1 : 0,
              s_lines == kMaxGeometryLines ? " (last GEO line this session)" : "");
}

}  // namespace ThiefHeadTracking
