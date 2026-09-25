// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "config.h"

#include "legacy_config/legacy_config.h"
#include "logging.h"

#include "cameraunlock/config/ini_reader.h"

#include <windows.h>

namespace ThiefHeadTracking {

namespace {

// The defaults WriteDefaultIni writes on first run live in config.h. The frozen reader in
// legacy_config/ holds its own copy, as the build it was taken from did.
//
// Note what is NOT here: the protocol-to-engine sign conversions. Position X and Z are
// mirrored relative to UE3 and both are converted at the engine boundary in
// camera_hook.cpp rather than through an Invert default, because inverting inside the
// pipeline flips the value BEFORE the asymmetric clamp and hands the generous
// forward-lean budget to the backward lean.

bool FileExists(const char* path) {
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

void WriteGeneralSection(cameraunlock::IniWriter& w) {
    w.WriteSection("General");
    w.WriteBool("EnableOnStartup", kDefaultEnableOnStartup);
    w.WriteInt("Port", kDefaultPort);
    w.WriteComment("Yaw mode: true = horizon-locked yaw (default), false = camera-local.");
    w.WriteBool("WorldSpaceYaw", kDefaultWorldSpaceYaw);
    w.WriteComment("Projects the game's aim point into the head-tracked view.");
    w.WriteComment("The reticle leaves the screen when the aim point is outside the view.");
    w.WriteBool("MoveCrosshair", kDefaultMoveCrosshair);
}

void WriteSensitivitySection(cameraunlock::IniWriter& w) {
    w.WriteSection("Sensitivity");
    w.WriteDouble("Yaw", kDefaultSensitivity);
    w.WriteDouble("Pitch", kDefaultSensitivity);
    w.WriteDouble("Roll", kDefaultSensitivity);
    w.WriteComment("Flip an axis only if your tracker reports it backwards. The engine's own");
    w.WriteComment("sign conventions are already handled; these three ship off.");
    w.WriteBool("InvertYaw", kDefaultInvert);
    w.WriteBool("InvertPitch", kDefaultInvert);
    w.WriteBool("InvertRoll", kDefaultInvert);
}

void WriteSmoothingSection(cameraunlock::IniWriter& w) {
    w.WriteSection("Smoothing");
    w.WriteComment("Chosen per connection from the tracker's source address; covers rotation and position.");
    w.WriteComment("LocalSmoothing: tracker running on this machine (loopback). 0 = none, 1 = heavy.");
    w.WriteDouble("LocalSmoothing", kDefaultLocalSmoothing);
    w.WriteComment("RemoteSmoothing: tracker on a remote network device. 0 = none, 1 = heavy.");
    w.WriteDouble("RemoteSmoothing", kDefaultRemoteSmoothing);
}

void WritePositionSection(cameraunlock::IniWriter& w) {
    w.WriteSection("Position");
    w.WriteComment("6DOF positional tracking. PositionScale = world units (cm) per metre of head translation.");
    w.WriteBool("Enabled", kDefaultPositionEnabled);
    w.WriteDouble("SensitivityX", kDefaultPosSens);
    w.WriteDouble("SensitivityY", kDefaultPosSens);
    w.WriteDouble("SensitivityZ", kDefaultPosSens);
    w.WriteDouble("LimitX", kDefaultPosLimitX);
    w.WriteDouble("LimitY", kDefaultPosLimitY);
    w.WriteDouble("LimitZ", kDefaultPosLimitZ);
    w.WriteDouble("LimitZBack", kDefaultPosLimitZBack);
    w.WriteDouble("PositionScale", kDefaultPositionScale);
}

void WriteCollisionSection(cameraunlock::IniWriter& w) {
    w.WriteSection("Collision");
    w.WriteComment("Trace positional lean against the level. Off by default.");
    w.WriteBool("Enabled", kDefaultCollision);
    w.WriteComment("World units (cm) kept between the camera and the surface it stopped at.");
    w.WriteDouble("Margin", kDefaultCollisionMargin);
    w.WriteComment("How quickly the lean reopens once whatever blocked it is gone.");
    w.WriteComment("0 = instantly, 1 = very slowly. Blocking is always immediate.");
    w.WriteDouble("ReleaseSmoothing", kDefaultCollisionRelease);
    w.WriteComment("Trace flags the world query is cast with. 0 uses the value for your");
    w.WriteComment("game build.");
    w.WriteHex("Channel", static_cast<int>(kDefaultCollisionChannel));
}

void WriteDiagnosticsSection(cameraunlock::IniWriter& w) {
    w.WriteSection("Diagnostics");
    w.WriteComment("Dumps the game structures this mod reads into HeadTracking.log, once");
    w.WriteComment("each. Leave it off unless a bug report asks for it: it makes the log");
    w.WriteComment("hundreds of lines longer and changes nothing about how the mod behaves.");
    w.WriteBool("StructProbe", kDefaultStructProbe);
}

void WriteHotkeysSection(cameraunlock::IniWriter& w) {
    w.WriteSection("Hotkeys");
    w.WriteComment("Virtual-key codes. Defaults: End (toggle), Page Up (cycle tracking mode), Page Down (yaw mode).");
    w.WriteHex("Toggle", kDefaultVkToggle);
    w.WriteHex("CycleMode", kDefaultVkCycleMode);
    w.WriteHex("YawMode", kDefaultVkYawMode);
    w.WriteComment("Chord alternatives: Ctrl+Shift+Y (toggle), Ctrl+Shift+G (cycle tracking mode), Ctrl+Shift+H (yaw mode).");
    w.WriteBool("ChordToggle", kDefaultChord);
    w.WriteBool("ChordCycleMode", kDefaultChord);
    w.WriteBool("ChordYawMode", kDefaultChord);
}

// Returns false when the file could not be created, so the caller can say WHY the
// config is missing. Without it the only diagnostic was "Failed to open INI", which
// names the symptom of a game directory the player cannot write to, not the cause.
bool WriteDefaultIni(const char* path) {
    cameraunlock::IniWriter w;
    if (!w.Open(path)) return false;
    w.WriteComment("Thief - Head Tracking configuration");
    w.WriteComment("Lives next to dinput8.dll in Binaries2/Win64/.");
    w.WriteBlankLine();
    WriteGeneralSection(w);
    w.WriteBlankLine();
    WriteSensitivitySection(w);
    w.WriteBlankLine();
    WriteSmoothingSection(w);
    w.WriteBlankLine();
    WritePositionSection(w);
    w.WriteBlankLine();
    WriteCollisionSection(w);
    w.WriteBlankLine();
    WriteHotkeysSection(w);
    w.WriteBlankLine();
    WriteDiagnosticsSection(w);
    w.Close();
    return true;
}

}  // namespace

bool Config::LoadOrCreate(const char* iniPath) {
    if (!iniPath || !*iniPath) {
        Log::Line("ERROR: could not resolve the directory this mod was loaded from, so "
                  "there is nowhere to read the INI from. The mod will not start.");
        return false;
    }
    if (!FileExists(iniPath) && !WriteDefaultIni(iniPath)) {
        Log::Line("ERROR: could not create the default INI at %s. The game directory is "
                  "not writable by this account.", iniPath);
        return false;
    }

    legacy::Config frozen;
    const legacy::ReadResult read = frozen.Read(iniPath);
    if (read.status == legacy::ReadStatus::Absent) {
        Log::Line("ERROR: Failed to open INI: %s", iniPath);
        return false;
    }
    if (read.status == legacy::ReadStatus::Refused) {
        return false;
    }

    enabled_on_startup = frozen.enabled_on_startup;
    udp_port = frozen.udp_port;
    sens_yaw = frozen.sens_yaw;
    sens_pitch = frozen.sens_pitch;
    sens_roll = frozen.sens_roll;
    invert_yaw = frozen.invert_yaw;
    invert_pitch = frozen.invert_pitch;
    invert_roll = frozen.invert_roll;
    local_smoothing = frozen.local_smoothing;
    remote_smoothing = frozen.remote_smoothing;
    move_crosshair = frozen.move_crosshair;
    world_space_yaw = frozen.world_space_yaw;
    position_enabled = frozen.position_enabled;
    pos_sens_x = frozen.pos_sens_x;
    pos_sens_y = frozen.pos_sens_y;
    pos_sens_z = frozen.pos_sens_z;
    pos_limit_x = frozen.pos_limit_x;
    pos_limit_y = frozen.pos_limit_y;
    pos_limit_z = frozen.pos_limit_z;
    pos_limit_z_back = frozen.pos_limit_z_back;
    position_scale = frozen.position_scale;
    collision_enabled = frozen.collision_enabled;
    collision_margin = frozen.collision_margin;
    collision_release_smoothing = frozen.collision_release_smoothing;
    collision_channel = frozen.collision_channel;
    struct_probe = frozen.struct_probe;
    vk_toggle = frozen.vk_toggle;
    vk_cycle_mode = frozen.vk_cycle_mode;
    vk_yaw_mode = frozen.vk_yaw_mode;
    chord_toggle = frozen.chord_toggle;
    chord_cycle_mode = frozen.chord_cycle_mode;
    chord_yaw_mode = frozen.chord_yaw_mode;
    return true;
}

}  // namespace ThiefHeadTracking
