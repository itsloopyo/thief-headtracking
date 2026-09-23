// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "camera_hook.h"

#include "ads.h"
#include "aim_marker.h"
#include "aim_trace.h"
#include "camera_collision.h"
#include "camera_diagnostics.h"
#include "fov_range.h"
#include "game_state.h"
#include "crosshair_hook.h"
#include "hud_basis.h"
#include "hook_install.h"
#include "logging.h"
#include "memory_probe.h"
#include "struct_probe.h"
#include "ue3_math.h"
#include "zoom_factor.h"

#include "cameraunlock/camera/zoom_compensation.h"

#include <windows.h>
#include <intrin.h>

#include <cmath>
#include <cstring>

#pragma intrinsic(_ReturnAddress)

namespace ThiefHeadTracking {

namespace {

// ULocalPlayer::CalcSceneView(FSceneViewFamily*, FVector& outViewLocation,
// FRotator& outViewRotation, FViewport*, FViewElementDrawer*) -> FSceneView*.
//
// It is hooked for one reason: to say which of its own callers is asking. The rendered
// view, a screen-to-world projection and the texture streamer all build a scene view
// through this one function, and only the first is what the player looks through.
using CalcSceneView_t = void*(__fastcall*)(void* self, void* family, void* outLoc,
                                           void* outRot, void* viewport, void* drawer);

// AController::eventGetPlayerViewPoint(FVector& outLocation, FRotator& outRotation).
// The shared "where is the player looking" accessor: the renderer, weapon aim,
// interaction traces, AI and audio all come through it.
using GetPlayerViewPoint_t = void(__fastcall*)(void* self, void* outLoc, void* outRot);

// APlayerController::GetFOVAngle(AController*) -> float degrees.
//
// Called rather than read out of the camera. It is what CalcSceneView calls two
// instructions after the viewpoint, so it answers with the exact field of view the
// projection matrix is built from - LockedFOV, every camera modifier and every script
// zoom included. Reading a POV-cache field at a guessed offset would agree with it in
// ordinary play and drift the moment anything zoomed, which is the failure that reads as
// the reticle being wrong only while aiming.
using GetFovAngle_t = float(__fastcall*)(void* controller);

CalcSceneView_t g_origCalcSceneView = nullptr;
GetPlayerViewPoint_t g_origGetViewPoint = nullptr;
TrackingRuntime* g_tracking = nullptr;

// Eases the lean out while the bow is drawn. Touched only from the viewpoint detour, which
// runs on the game thread for the scene-view caller alone, so it needs no synchronisation
// of its own - the same restriction TrackingRuntime::SampleFrame relies on.
LeanEase g_leanEase;

// Everything the detours read that is only known once the build profile is matched and
// the config is loaded. Written once at install time, before either detour is enabled,
// and read on the game thread from then on.
struct HookSettings {
    const BuildProfile* profile = nullptr;
    std::uintptr_t moduleBase = 0;
    std::uintptr_t calcSceneViewReturn = 0;
    std::uintptr_t deProjectCaller = 0;
    std::uintptr_t streamingCaller = 0;
    GetFovAngle_t getFovAngle = nullptr;
    std::uint32_t offPlayerCamera = 0;
    std::uint32_t offCamDefaultFov = 0;
    float positionScale = 0.0f;
    bool moveCrosshair = false;
};
HookSettings g_hook;

// How deep inside CalcSceneView this thread is, and whether the outermost one it is
// inside is the rendered view. Thread-local because the scene view is built on the game
// thread while other threads keep asking for the viewpoint for their own reasons.
thread_local int t_sceneViewDepth = 0;
thread_local bool t_sceneViewIsDraw = false;
thread_local bool t_viewInjected = false;

// How much of each engine object the probe dumps when diagnostics are on. Both are wide
// enough to reach every offset the build profile pins on that object, which is what a
// session re-derives them from after a patch.
constexpr std::uint32_t kCameraProbeBytes = 0x400u;
constexpr std::uint32_t kControllerProbeBytes = 0x600u;

float ReadFloat(const std::uint8_t* p, std::uint32_t off) {
    return *reinterpret_cast<const float*>(p + off);
}

// The camera the scene view is being built for, reached through the controller
// CalcSceneView asked the viewpoint of. No camera search and no cached pointer, so
// nothing here can go stale across a level load.
//
// Both hops are validated, and the second is validated for the span the caller actually
// reads rather than for the first word of it: DefaultFOV sits 0x2A4 into the camera, far
// enough in to be on a different page from the one the pointer lands on.
const std::uint8_t* PlayerCamera(const std::uint8_t* controller) {
    const auto field = reinterpret_cast<std::uintptr_t>(controller) + g_hook.offPlayerCamera;
    if (!ReadableSpan(field, sizeof(std::uintptr_t))) {
        return nullptr;
    }
    const auto cam = *reinterpret_cast<const std::uintptr_t*>(field);
    if (!Readable(cam, g_hook.offCamDefaultFov + sizeof(float))) {
        return nullptr;
    }
    return reinterpret_cast<const std::uint8_t*>(cam);
}

// Suppress injection in every state that is not active gameplay, and wherever the
// viewpoint cannot be read safely.
std::uint32_t ReadGate(const std::uint8_t* controller, const UE3Vector* loc, float* outFov,
                       float* outBaseFov, float* outConstrainedAspect, bool* outAiming,
                       const std::uint8_t** outCamera) {
    // Cleared before the first thing that can return, so the caller cannot read last
    // frame's values - the aim state especially - off an early exit.
    *outFov = 0.0f;
    *outBaseFov = 0.0f;
    *outConstrainedAspect = 0.0f;
    *outAiming = false;
    *outCamera = nullptr;

    const std::uint8_t* cam = PlayerCamera(controller);
    if (cam == nullptr) {
        return kGateNoCamera;
    }
    *outCamera = cam;
    ProbeStruct("camera", cam, kCameraProbeBytes);

    // The field of view the frame is rendered at, asked of the game the way CalcSceneView
    // asks a moment later. DefaultFOV comes off the camera because it is a plain field and
    // there is no accessor for it; it is only the unzoomed BASE, never what is rendered.
    const float fov = g_hook.getFovAngle
                          ? g_hook.getFovAngle(const_cast<std::uint8_t*>(controller))
                          : 0.0f;
    *outFov = fov;
    *outBaseFov = ReadFloat(cam, g_hook.offCamDefaultFov);
    LogZoomBasis(fov, *outBaseFov);

    std::uint32_t bits = 0;
    if (!AllFinite(loc->X, loc->Y, loc->Z)) {
        bits |= kGateBadPov;
    }
    // The FOV gates the frame only where the accessor is pinned, so a build that pinned
    // everything else still head-tracks. Gating on it unconditionally means a profile with
    // rvaGetFovAngle left at 0 reads back 0.0 here, closes the gate on every single frame,
    // and reports it as a bad viewpoint - the mod silently does nothing with no line in the
    // log naming the actual cause. Unpinned costs the zoom compensation and the reticle,
    // both of which already stand down on an unusable FOV of their own accord.
    if (g_hook.getFovAngle && !IsUsableFov(fov)) {
        bits |= kGateBadPov;
    }
    bits |= ReadGameStateGate(controller, outAiming);
    return bits;
}

// The frame this call belongs to is not being rendered, or is not one to inject into:
// hide the marker so the HUD leaves the reticle alone, drop the lean allowance so the
// previous room's wall does not ration the next lean, and keep counting for the traffic
// report.
void StandDown() {
    GetAimMarker().active.store(false, std::memory_order_relaxed);
    ResetCameraCollision();
    // The game's own matrix stands while nothing is being injected, so the HUD projects
    // from the view the game chose - which is what an unmodded frame does.
    ClearHudPose();
    LogTraffic(true, false);
}

// Publishes where the shot lands, in the head-tracked view, so the HUD hook can put the
// reticle there.
//
// The vector is taken from the RENDER eye to the impact point rather than along the clean
// aim direction. That is what makes the reticle stay on its target at every range instead
// of only at one: a lean moves the rendered eye up to 30 cm off the eye the shot leaves
// from, and from there a direction and a point no longer project to the same pixel.
//
// A definite no-hit projects the clean aim direction instead, which is the same
// arithmetic with the target at infinity. A trace that could not run publishes nothing:
// the reticle is hidden and the failure is logged, rather than a magic
// distance being substituted.
void PublishAimMarker(const void* controller, const UE3Vector& cleanEye, const UE3Rotator& clean,
                      const UE3Vector& renderEye, const UE3Rotator& tracked, float fov,
                      float constrainedAspect, const float leanRuf[3]) {
    const auto pawnField = reinterpret_cast<std::uintptr_t>(controller) + g_hook.profile->offControllerPawn;
    const void* pawn = nullptr;
    if (ReadableSpan(pawnField, sizeof(pawn))) {
        std::memcpy(&pawn, reinterpret_cast<const void*>(pawnField), sizeof(pawn));
    }
    const AimPoint aim = TraceAim(cleanEye, clean, pawn);
    if (!aim.queried) {
        static bool s_warned = false;
        if (!s_warned) {
            s_warned = true;
            Log::Line("WARN: the aim trace could not run; hiding the reticle while tracking");
        }
        GetAimMarker().active.store(false, std::memory_order_relaxed);
        return;
    }

    float ruf[3];
    if (aim.hit) {
        ResolvePointInTrackedView(renderEye, aim.point, tracked, ruf);
    } else {
        ResolveAimInTrackedView(clean, tracked, ruf);
    }

    AimMarker& marker = GetAimMarker();
    marker.right.store(ruf[0], std::memory_order_relaxed);
    marker.up.store(ruf[1], std::memory_order_relaxed);
    marker.forward.store(ruf[2], std::memory_order_relaxed);
    marker.fov_deg.store(fov, std::memory_order_relaxed);
    marker.constrained_aspect.store(constrainedAspect, std::memory_order_relaxed);
    marker.distance.store(aim.hit ? aim.distance : 0.0f, std::memory_order_relaxed);
    marker.lean_right.store(leanRuf[0], std::memory_order_relaxed);
    marker.lean_up.store(leanRuf[1], std::memory_order_relaxed);
    marker.lean_forward.store(leanRuf[2], std::memory_order_relaxed);
    marker.has_point.store(aim.hit, std::memory_order_relaxed);
    marker.active.store(true, std::memory_order_relaxed);
}

// Moves the viewpoint by the tracked head position, and reports the lean it applied in
// the engine's own right/up/forward basis so the marker can publish it.
//
// @p zoom is this frame's zoom compensation. A lean shifts the image by the parallax it
// opens up, which the projection scales by 1/tan(fov/2) exactly as it scales a rotation,
// so leaning is amplified by a zoom the same way turning is and is scaled back by the
// same factor.
void ApplyPositionOffset(const FrameSample& s, const UE3Rotator& clean, float zoom,
                         UE3Vector* loc, float leanRuf[3]) {
    // Horizon-locked basis, built from the CLEAN yaw alone: forward = (cy, sy, 0),
    // right = (-sy, cy, 0), up = world +Z. Carrying the clean pitch into the forward
    // vector makes the three axes non-orthogonal and turns a forward lean into a descent.
    const float yawRad = static_cast<float>(clean.Yaw) * kUnitsToRad;
    const float cy = std::cos(yawRad), sy = std::sin(yawRad);

    // Protocol-to-engine axis conversion, done HERE and only here, and inherited from
    // dishonored-headtracking, which hooks this same UE3 accessor and had its signs
    // settled in a running game. The core's convention is that negative z is the forward
    // lean, which is what puts the generous LimitZ (0.40 m) on leaning in and the
    // restricted LimitZBack (0.10 m) on pulling away; UE3's camera-local +X is forward,
    // so the sign flips at this boundary, AFTER the clamp rather than before it. Doing it
    // with the processor's invert_z instead flips the value ahead of the clamp and hands
    // the 0.40 m to the backward lean, which reads in game as "leaning in barely moves,
    // pulling back moves a lot". x is mirrored the same way and is converted in the same
    // place; its clamp is symmetric so only the direction changes.
    const float scale = g_hook.positionScale * zoom;
    const float oR = -s.pos_x * scale;
    const float oU =  s.pos_y * scale;
    const float oF = -s.pos_z * scale;

    float dx = cy * oF - sy * oR;
    float dy = sy * oF + cy * oR;
    float dz = oU;

    // Cut back to whatever the level leaves room for, before it is applied. The sweep
    // starts from the CLEAN eye - clamping afterwards would mean reading back a position
    // that is already inside the wall.
    ClampLean(*loc, &dx, &dy, &dz);

    // Report the lean that was actually applied, resolved back onto the same basis it was
    // built from, so the diagnostic line describes the camera the player is looking
    // through rather than the pose the tracker asked for.
    leanRuf[0] = -sy * dx + cy * dy;
    leanRuf[1] = dz;
    leanRuf[2] = cy * dx + sy * dy;

    loc->X += dx;
    loc->Y += dy;
    loc->Z += dz;
}

// The zoom scale, applied only where its arithmetic is defined.
//
// Core's ScaleAngleForZoom is atan(tan(angle) * factor), and its contract says the angle
// must be inside +/-90. Nothing upstream keeps it there: the processor decomposes yaw into
// (-180, 180] and the INI sensitivity then multiplies it. Out of that range the tangent
// wraps rather than saturating, so 120 degrees of head yaw comes back as -60 and the view
// snaps to the opposite side of the player as the head crosses 90 - a 180 degree flip
// between two frames. Saturating at the bound is monotonic, so the view stops turning
// instead of reversing, and turning the head back recovers it.
//
// A factor of exactly 1, which is every frame of ordinary play, returns the angle
// untouched. That is what the tangent round trip already does inside the range, so this
// only extends the identity to the angles the round trip cannot represent.
constexpr float kMaxZoomScaledDegrees = 89.0f;

float ScaleForZoom(float angleDegrees, float zoom) {
    if (zoom == 1.0f) {
        return angleDegrees;
    }
    if (angleDegrees > kMaxZoomScaledDegrees) {
        return cameraunlock::camera::ScaleAngleForZoom(kMaxZoomScaledDegrees, zoom);
    }
    if (angleDegrees < -kMaxZoomScaledDegrees) {
        return cameraunlock::camera::ScaleAngleForZoom(-kMaxZoomScaledDegrees, zoom);
    }
    return cameraunlock::camera::ScaleAngleForZoom(angleDegrees, zoom);
}

// Adds the tracked head rotation to the viewpoint. The composition, the engine's roll
// convention and the pitch bound all live in ue3_math.h so they are testable.
//
// Yaw and pitch are scaled by @p zoom. Roll is not: a head tilt turns the image by its own
// angle whatever the field of view, so there is nothing for a zoom to amplify and scaling
// it would under-tilt the view.
void ApplyHeadRotation(const FrameSample& s, const UE3Rotator& clean, bool worldSpaceYaw,
                       float zoom, UE3Rotator* rot) {
    ComposeHeadRotation(clean, ScaleForZoom(s.pitch, zoom), ScaleForZoom(s.yaw, zoom),
                        s.roll, worldSpaceYaw, rot);
}

void __fastcall GetPlayerViewPointDetour(void* self, void* outLoc, void* outRot) {
    const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());

    g_origGetViewPoint(self, outLoc, outRot);

    // Every caller but the rendered scene view keeps the rotation the mouse chose. That is
    // the whole of aim decoupling: weapon fire, interaction traces, AI vision and audio
    // all read this function, and they read it clean.
    const bool fromSceneView = t_sceneViewDepth > 0 && t_sceneViewIsDraw &&
                               caller == g_hook.calcSceneViewReturn;
    // The runtime is set as the first statement of InstallCameraHook, before either detour
    // is enabled, and is never cleared: the module is pinned and DLL_PROCESS_DETACH does
    // nothing, so there is no teardown for a game thread inside this function to race.
    TrackingRuntime* const tracking = g_tracking;
    if (!self || !fromSceneView) {
        LogTraffic(fromSceneView, false);
        return;
    }

    auto* loc = static_cast<UE3Vector*>(outLoc);
    auto* rot = static_cast<UE3Rotator*>(outRot);
    const UE3Vector cleanEye = *loc;
    const UE3Rotator clean = *rot;

    ProbeStruct("controller", self, kControllerProbeBytes);

    float fov = 0.0f, baseFov = 0.0f, constrainedAspect = 0.0f;
    bool aiming = false;
    const std::uint8_t* cam = nullptr;
    std::uint32_t gate =
        ReadGate(static_cast<const std::uint8_t*>(self), loc, &fov, &baseFov,
                 &constrainedAspect, &aiming, &cam);

    ReportGate(gate, fov);
    if (gate != 0) {
        g_leanEase.Reset();
        StandDown();
        return;
    }

    FrameSample s = tracking->SampleFrame();
    if (!s.has_rotation && !s.has_position) {
        g_leanEase.Reset();
        StandDown();
        return;
    }

    const float zoom = ZoomFactor(fov, baseFov);

    float leanRuf[3] = { 0.0f, 0.0f, 0.0f };
    if (s.has_position) {
        // Polled from the game every frame rather than latched, so a missed edge heals on
        // the next one. Rotation is deliberately left out of this.
        const float leanScale = g_leanEase.Update(aiming, GetTickCount64());
        s.pos_x *= leanScale;
        s.pos_y *= leanScale;
        s.pos_z *= leanScale;
        ApplyPositionOffset(s, clean, zoom, loc, leanRuf);
    } else {
        g_leanEase.Reset();
        ResetCameraCollision();
    }
    if (s.has_rotation) {
        ApplyHeadRotation(s, clean, tracking->IsWorldSpaceYaw(), zoom, rot);
    }

    t_viewInjected = true;
    if (g_hook.moveCrosshair) {
        PublishAimMarker(self, cleanEye, clean, *loc, *rot, fov, constrainedAspect, leanRuf);
    }
    // The HUD projects its world anchors through a matrix on the camera, and that matrix
    // has to describe the view the frame is drawn from or every marker sits still while the
    // world moves under it. The eye delta carries the lean, so an anchor moves with a lean
    // as well as with a turn.
    const float eyeDelta[3] = { loc->X - cleanEye.X, loc->Y - cleanEye.Y,
                                loc->Z - cleanEye.Z };
    if (cam != nullptr) {
        SetHudPose(cam, *rot, eyeDelta);
    }
    ReportGeometry(s, clean, *rot, cleanEye, *loc, leanRuf, fov, zoom);
    LogTraffic(true, true);
}

void* __fastcall CalcSceneViewDetour(void* self, void* family, void* outLoc, void* outRot,
                                     void* viewport, void* drawer) {
    const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());

    // Fails OPEN, and deliberately: the two callers that must not be head-tracked are
    // named, and anything else is treated as the render. A wrong read here that failed
    // closed would switch head tracking off for the whole session with nothing in the log
    // to say why, which is far worse than a screen-to-world query briefly seeing a tracked
    // view.
    const bool isDraw = caller != g_hook.deProjectCaller && caller != g_hook.streamingCaller;
    LogSceneViewCaller(caller, g_hook.moduleBase, isDraw);

    const bool prevIsDraw = t_sceneViewIsDraw;
    const bool prevInjected = t_viewInjected;
    t_viewInjected = false;
    ++t_sceneViewDepth;
    t_sceneViewIsDraw = isDraw;
    void* view = g_origCalcSceneView(self, family, outLoc, outRot, viewport, drawer);
    if (isDraw && view) UpdateCrosshair(view, t_viewInjected);
    t_viewInjected = prevInjected;
    t_sceneViewIsDraw = prevIsDraw;
    --t_sceneViewDepth;
    return view;
}

}  // namespace

bool InstallCameraHook(const BuildProfile& profile, std::uintptr_t moduleBase,
                       TrackingRuntime& tracking, const Config& cfg, bool reticleAvailable) {
    g_tracking = &tracking;
    g_hook.profile = &profile;
    g_hook.moduleBase = moduleBase;
    g_hook.calcSceneViewReturn = moduleBase + profile.rvaCalcSceneViewReturn;
    g_hook.deProjectCaller = profile.rvaDeProjectCaller
                                 ? moduleBase + profile.rvaDeProjectCaller : 0;
    g_hook.streamingCaller = profile.rvaStreamingCaller
                                 ? moduleBase + profile.rvaStreamingCaller : 0;
    g_hook.offPlayerCamera = profile.offPlayerCamera;
    g_hook.offCamDefaultFov = profile.offCamDefaultFov;
    g_hook.getFovAngle = profile.rvaGetFovAngle
                             ? reinterpret_cast<GetFovAngle_t>(moduleBase + profile.rvaGetFovAngle)
                             : nullptr;
    g_hook.positionScale = cfg.position_scale;
    // Only where something reads it. The marker costs a full-length world trace inside the
    // viewpoint accessor on every rendered frame, and with no reticle hook installed the
    // only reader is a throttled log line. It would also blame the aim trace, in a WARN, for
    // a reticle that is standing down because no hook was ever built for this build.
    g_hook.moveCrosshair = cfg.move_crosshair && reticleAvailable;

    // The caller filter has to be live before the viewpoint hook can trust it, so this one
    // goes first.
    const std::uintptr_t calcSceneView = moduleBase + profile.rvaCalcSceneView;
    if (!InstallDetour(calcSceneView, reinterpret_cast<void*>(&CalcSceneViewDetour),
                       reinterpret_cast<void**>(&g_origCalcSceneView),
                       "ULocalPlayer::CalcSceneView")) {
        return false;
    }

    const std::uintptr_t viewPoint = moduleBase + profile.rvaGetPlayerViewPoint;
    if (!InstallDetour(viewPoint, reinterpret_cast<void*>(&GetPlayerViewPointDetour),
                       reinterpret_cast<void**>(&g_origGetViewPoint),
                       "AController::eventGetPlayerViewPoint")) {
        return false;
    }

    if (!InstallHudBasisHook(profile, moduleBase)) {
        Log::Line("WARN: quest, awareness and interaction markers will hold their place on "
                  "screen while the head turns");
    }

    if (!g_hook.getFovAngle) {
        Log::Line("WARN: the field-of-view accessor is not pinned on this build. Head "
                  "tracking still runs, but it is not scaled back when the game narrows "
                  "the view, so it will feel exaggerated through a zoom.");
    }
    Log::Line("Camera hook installed: CalcSceneView @ 0x%p, eventGetPlayerViewPoint @ 0x%p "
              "(scene-view caller returns to 0x%p)",
              reinterpret_cast<void*>(calcSceneView), reinterpret_cast<void*>(viewPoint),
              reinterpret_cast<void*>(g_hook.calcSceneViewReturn));
    return true;
}

}  // namespace ThiefHeadTracking
