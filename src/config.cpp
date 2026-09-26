// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "config.h"

#include "legacy_config/legacy_config.h"
#include "path_utils.h"
#include "ue3_math.h"

#include "cameraunlock/config/head_tracking_config_table.h"
#include "cameraunlock/input/key_bindings.h"
#include "cameraunlock/tracking/tracking_mode.h"

#include <string>
#include <utility>
#include <vector>

namespace ThiefHeadTracking {

namespace {

using cameraunlock::config::DropRule;
using cameraunlock::config::DroppedValue;
using cameraunlock::config::ImportResult;
using cameraunlock::config::LegacyInput;
using cameraunlock::config::LegacyPoseShaping;
using cameraunlock::config::PoseShapingValue;
using cameraunlock::input::KeyBinding;
using cameraunlock::input::KeyModifiers;

// A legacy hotkey code and its Ctrl+Shift chord switch as one key list: the code's binding
// when it is a key code, then the chord.
std::string KeyList(int vk, bool chord, char letter, const char* key, std::vector<DroppedValue>& dropped) {
    cameraunlock::config::LegacyVirtualKeyToBindings(vk, "Hotkeys", key, dropped);
    std::vector<KeyBinding> bindings;
    if (vk >= 0x01 && vk <= 0xFE) bindings.push_back({KeyModifiers::kNone, vk});
    if (chord) bindings.push_back({KeyModifiers::kCtrl | KeyModifiers::kShift, letter});
    return cameraunlock::input::FormatKeyBindings(bindings);
}

ImportResult Import(const LegacyInput& input, Config& out) {
    // The published build opened the file by the ANSI path it built itself, not the one the
    // owner derives, so the import builds it the same way.
    const std::string ansiPath = LegacyAnsiPath(input.path);
    if (ansiPath.empty()) {
        return ImportResult::Refused(
            "its path has no ANSI form within MAX_PATH, so the version that wrote this file could "
            "not open it and did not start");
    }

    legacy::Config c;
    const legacy::ReadResult read = c.Read(ansiPath.c_str());
    if (read.status == legacy::ReadStatus::Refused) {
        return ImportResult::Refused(read.reason);
    }

    std::vector<DroppedValue> dropped;
    std::vector<PoseShapingValue> shaping;

    out.enable_on_startup = c.enabled_on_startup;
    out.udp_port = c.udp_port;
    out.world_space_yaw = c.world_space_yaw;

    // [Position] Enabled chose only the startup mode: the cycle key reached every mode
    // either way.
    const cameraunlock::TrackingModeChannels mode = cameraunlock::EncodeTrackingMode(
        c.position_enabled ? cameraunlock::TrackingMode::RotationAndPosition
                           : cameraunlock::TrackingMode::RotationOnly);
    out.rotation_enabled = mode.rotation_enabled;
    out.position_enabled = mode.position_enabled;

    out.local_smoothing = c.local_smoothing;
    out.position.local_smoothing = c.local_smoothing;
    out.remote_smoothing = c.remote_smoothing;
    out.position.remote_smoothing = c.remote_smoothing;

    // The old file had one vertical limit, which the old runtime applied both ways.
    out.position.limit_x = c.pos_limit_x;
    out.position.limit_y = c.pos_limit_y;
    out.position.limit_y_down = c.pos_limit_y;
    out.position.limit_z = c.pos_limit_z;
    out.position.limit_z_back = c.pos_limit_z_back;

    // The lean clamp shipped switched off pending verification in this game, so it takes the
    // table's default (approved change follows_default).
    out.collision_enabled = MakeConfigTable().defaults().collision_enabled;
    if (c.collision_enabled != out.collision_enabled) {
        dropped.push_back({DropRule::FollowsDefault, "Collision", "Enabled", c.collision_enabled ? "true" : "false"});
    }
    out.lean_clamp.skin = c.collision_margin;
    out.lean_clamp.release_smoothing = c.collision_release_smoothing;
    // The field holds the flag word's 32 bits; the camera collision reads them back unsigned.
    out.collision_channel = static_cast<int>(c.collision_channel);

    out.struct_probe = c.struct_probe;

    // Every rotation sensitivity and inversion and every position sensitivity shipped at
    // identity, so nothing folds and a value the player changed is dropped. The unit scale
    // shipped at the centimetres per metre the camera hook now applies itself.
    LegacyPoseShaping(c.sens_yaw, 1.0f, "Sensitivity", "Yaw", shaping, dropped);
    LegacyPoseShaping(c.sens_pitch, 1.0f, "Sensitivity", "Pitch", shaping, dropped);
    LegacyPoseShaping(c.sens_roll, 1.0f, "Sensitivity", "Roll", shaping, dropped);
    LegacyPoseShaping(c.invert_yaw, false, "Sensitivity", "InvertYaw", shaping, dropped);
    LegacyPoseShaping(c.invert_pitch, false, "Sensitivity", "InvertPitch", shaping, dropped);
    LegacyPoseShaping(c.invert_roll, false, "Sensitivity", "InvertRoll", shaping, dropped);
    LegacyPoseShaping(c.pos_sens_x, 1.0f, "Position", "SensitivityX", shaping, dropped);
    LegacyPoseShaping(c.pos_sens_y, 1.0f, "Position", "SensitivityY", shaping, dropped);
    LegacyPoseShaping(c.pos_sens_z, 1.0f, "Position", "SensitivityZ", shaping, dropped);
    LegacyPoseShaping(c.position_scale, kWorldUnitsPerMetre, "Position", "PositionScale", shaping, dropped);

    // The game's reticle now always follows the aim.
    if (!c.move_crosshair) dropped.push_back({DropRule::Reticle, "General", "MoveCrosshair", "false"});

    out.toggle_key_name = KeyList(c.vk_toggle, c.chord_toggle, 'Y', "Toggle", dropped);
    out.cycle_tracking_mode_key_name = KeyList(c.vk_cycle_mode, c.chord_cycle_mode, 'G', "CycleMode", dropped);
    out.yaw_mode_key_name = KeyList(c.vk_yaw_mode, c.chord_yaw_mode, 'H', "YawMode", dropped);

    return read.status == legacy::ReadStatus::Absent ? ImportResult::Absent(std::move(dropped), std::move(shaping))
                                                     : ImportResult::Imported(std::move(dropped), std::move(shaping));
}

}  // namespace

cameraunlock::config::ConfigTable<Config> MakeConfigTable() {
    using cameraunlock::config::schema::Concept;
    cameraunlock::config::ConfigTable<Config> table = cameraunlock::config::HeadTrackingConfigTable<Config>(
        {Concept::UdpPort, Concept::EnableOnStartup, Concept::WorldSpaceYaw, Concept::RotationEnabled,
         Concept::LocalSmoothing, Concept::RemoteSmoothing, Concept::PositionEnabled, Concept::PositionLimitX,
         Concept::PositionLimitY, Concept::PositionLimitYDown, Concept::PositionLimitZ, Concept::PositionLimitZBack,
         Concept::CollisionEnabled, Concept::CollisionMargin, Concept::CollisionChannel,
         Concept::CollisionReleaseSmoothing, Concept::ToggleKey, Concept::CycleTrackingModeKey,
         Concept::YawModeKey});
    table.Select(Concept::WorldSpaceYaw).Writable()
        .Select(Concept::RotationEnabled).Writable()
        .Select(Concept::PositionEnabled).Writable();
    table.Select(Concept::CollisionMargin)
        .Comment("How far, in centimetres, the view is held off a wall when you lean into it.");
    table.Select(Concept::CollisionChannel)
        .Comment("The trace flags the wall check is cast with, as a decimal number.\n"
                 "0 uses the flags pinned for your game build.");
    table.Local("Diagnostics", "StructProbe", &Config::struct_probe, cameraunlock::config::BoolCodec(),
                "true: write the game structures this mod reads to HeadTracking.log, once each.\n"
                "Leave it off unless a bug report asks for it: it makes the log hundreds of lines longer.");
    return table;
}

cameraunlock::config::LegacyImport<Config> MakeLegacyImport() {
    return {&Import, legacy::ReadKeys()};
}

cameraunlock::config::ConfigOwnerOptions<Config> MakeConfigOwnerOptions(const std::wstring& folder,
                                                                        cameraunlock::config::DefaultsFile defaults) {
    const auto wide = [](const char* name) { return std::wstring(name, name + std::char_traits<char>::length(name)); };
    cameraunlock::config::ConfigOwnerOptions<Config> options;
    options.path = folder + wide(kConfigFileName);
    options.legacy_path = folder + wide(kLegacyConfigFileName);
    options.table = MakeConfigTable();
    options.import = MakeLegacyImport();
    options.header.display_name = kConfigDisplayName;
    options.defaults = std::move(defaults);
    return options;
}

}  // namespace ThiefHeadTracking
