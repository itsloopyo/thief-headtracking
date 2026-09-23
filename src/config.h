// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "cameraunlock/camera/lean_clamp.h"
#include "cameraunlock/data/position_settings.h"
#include "cameraunlock/math/smoothing_utils.h"

#include <cstdint>

namespace ThiefHeadTracking {

// The shipped defaults, in one place.
//
// These are the single source of truth for three readers that would otherwise keep their
// own copies and drift apart silently: the INI writer, the INI reader's per-key fallback,
// and the Config member initialisers below. A test can pin them because they are declared
// here rather than in an anonymous namespace in the .cpp.
constexpr bool  kDefaultEnableOnStartup = true;
constexpr int   kDefaultPort            = 4242;
constexpr int   kMinPort                = 1024;
constexpr int   kMaxPort                = 65535;
constexpr bool  kDefaultWorldSpaceYaw   = true;
constexpr bool  kDefaultMoveCrosshair   = true;
constexpr float kDefaultSensitivity     = 1.0f;
constexpr bool  kDefaultInvert          = false;
constexpr float kDefaultLocalSmoothing  = static_cast<float>(cameraunlock::math::kDefaultLocalSmoothing);
constexpr float kDefaultRemoteSmoothing = static_cast<float>(cameraunlock::math::kDefaultRemoteSmoothing);

constexpr bool  kDefaultPositionEnabled = true;
constexpr float kDefaultPosSens         = 1.0f;
constexpr float kDefaultPosLimitX       = cameraunlock::PositionSettings{}.limit_x;
constexpr float kDefaultPosLimitY       = cameraunlock::PositionSettings{}.limit_y;
constexpr float kDefaultPosLimitZ       = cameraunlock::PositionSettings{}.limit_z;
constexpr float kDefaultPosLimitZBack   = cameraunlock::PositionSettings{}.limit_z_back;
// UE3 worlds are in centimetres, and the tracker reports metres.
constexpr float kDefaultPositionScale   = 100.0f;

// Head tracking moves the rendered eye off the player's own, so a lean toward a wall can
// carry it through the surface. The clamp traces the lean against the world and stops the
// eye short of what it would have entered.
// OFF until the world trace has been seen to engage in this game. The doctrine's rule is
// that the clamp ships disabled until a log shows it stopping a lean at a real wall, and on
// this build the trace has never returned a hit at all - so turning it on would cost a
// trace on every rendered frame and change nothing. Leaning is unclamped until then, which
// means a hard lean into a wall can put the view inside it.
constexpr bool  kDefaultCollision       = false;
// World units (cm) held between the eye and the surface it stopped at, measured along
// that surface's normal. It has to exceed the near clip plane or the wall is culled and
// the player sees through it anyway. This game's near plane has not been read out of the
// running engine, so 20 is a deliberately generous choice rather than a derived one - it
// clears UE3's own compiled-in default several times over. Read the real number before
// tightening it.
constexpr float kDefaultCollisionMargin = 20.0f;
// The trace flag word the world query is cast with. 0 means "use the value the build
// profile pinned", which is what every player should be on; a non-zero value here
// overrides it, and exists because which flags level geometry blocks is a project setting
// rather than an engine constant. The log reports which value was used and the GEO line
// reports whether the trace is hitting anything, so a wrong pin can be found by trying.
constexpr std::uint32_t kDefaultCollisionChannel = 0u;
constexpr float kDefaultCollisionRelease =
    cameraunlock::camera::LeanClampSettings{}.release_smoothing;

// Dumps the engine objects the hooks read - the controller, its camera, the world info -
// to the log, once each. It is how the struct offsets in the build profile get re-pinned
// after a patch moves them, and it is off because the dump is hundreds of lines long.
constexpr bool  kDefaultStructProbe     = false;

constexpr int   kDefaultVkToggle        = 0x23; // VK_END
constexpr int   kDefaultVkCycleMode     = 0x21; // VK_PRIOR (Page Up)
constexpr int   kDefaultVkYawMode       = 0x22; // VK_NEXT (Page Down)
constexpr bool  kDefaultChord           = true;

struct Config {
    bool  enabled_on_startup = kDefaultEnableOnStartup;
    std::uint16_t udp_port = kDefaultPort;

    float sens_yaw = kDefaultSensitivity;
    float sens_pitch = kDefaultSensitivity;
    float sens_roll = kDefaultSensitivity;
    // Per-axis user inversion, for a tracker that genuinely reports an axis backwards.
    // The protocol-to-engine sign conversion is NOT carried here: the position axes are
    // mirrored and are converted at the engine boundary in camera_hook.cpp, so all three
    // of these mean the same thing and all three ship off.
    bool  invert_yaw = kDefaultInvert;
    bool  invert_pitch = kDefaultInvert;
    bool  invert_roll = kDefaultInvert;

    // Smoothing is picked per connection from the packet source address: loopback senders
    // get local_smoothing, remote network devices get remote_smoothing. Both cover
    // rotation and position.
    float local_smoothing = kDefaultLocalSmoothing;
    float remote_smoothing = kDefaultRemoteSmoothing;

    // The game draws its reticle at screen centre, which is no longer where the game is
    // aiming once the view is head-tracked. This moves it to the point the shot lands on.
    // Inert until a build profile pins a reticle update to hook; with none, the aim marker
    // is not published either.
    bool move_crosshair = kDefaultMoveCrosshair;

    // true = horizon-locked (world-space) yaw, false = camera-local.
    bool world_space_yaw = kDefaultWorldSpaceYaw;

    // 6DOF positional tracking.
    bool  position_enabled = kDefaultPositionEnabled;
    float pos_sens_x = kDefaultPosSens;
    float pos_sens_y = kDefaultPosSens;
    float pos_sens_z = kDefaultPosSens;
    float pos_limit_x = kDefaultPosLimitX;
    float pos_limit_y = kDefaultPosLimitY;
    float pos_limit_z = kDefaultPosLimitZ;
    float pos_limit_z_back = kDefaultPosLimitZBack;
    float position_scale = kDefaultPositionScale;

    // Keeps the head-tracked eye out of the world. See kDefaultCollision.
    bool  collision_enabled = kDefaultCollision;
    float collision_margin = kDefaultCollisionMargin;
    float collision_release_smoothing = kDefaultCollisionRelease;
    std::uint32_t collision_channel = kDefaultCollisionChannel;

    // Diagnostics. See kDefaultStructProbe.
    bool struct_probe = kDefaultStructProbe;

    int vk_toggle     = kDefaultVkToggle;
    int vk_cycle_mode = kDefaultVkCycleMode;
    int vk_yaw_mode   = kDefaultVkYawMode;
    bool chord_toggle = kDefaultChord;
    bool chord_cycle_mode = kDefaultChord;
    bool chord_yaw_mode = kDefaultChord;

    bool LoadOrCreate(const char* iniPath);
};

}  // namespace ThiefHeadTracking
