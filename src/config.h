// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "cameraunlock/config/config_owner.h"
#include "cameraunlock/config/config_table.h"
#include "cameraunlock/config/defaults_file.h"
#include "cameraunlock/config/head_tracking_config.h"
#include "cameraunlock/config/legacy_import.h"

#include <string>

namespace ThiefHeadTracking {

constexpr const char* kConfigFileName = "CameraUnlock.ini";
// The file every build before the canonical format read, beside kConfigFileName. Imported once
// while kConfigFileName is absent, and never written.
constexpr const char* kLegacyConfigFileName = "ThiefHeadTracking.ini";
// The game's name as cameraunlock-core's data/games.json spells it.
constexpr const char* kConfigDisplayName = "Thief";

// Core's config with this game's defaults.
struct Config : cameraunlock::HeadTrackingConfig {
    // Dumps the engine objects the hooks read - the controller, its camera, the world info -
    // to the log, once each. It is how the struct offsets in the build profile get re-pinned
    // after a patch moves them, and it is off because the dump is hundreds of lines long.
    bool struct_probe = false;

    Config() {
        // World units (cm) held between the eye and the surface it stopped at, measured
        // along that surface's normal. It has to exceed the near clip plane or the wall is
        // culled and the player sees through it anyway. This game's near plane has not been
        // read out of the running engine, so 20 is a deliberately generous choice rather
        // than a derived one - it clears UE3's own compiled-in default several times over.
        lean_clamp.skin = 20.0f;
    }
};

// The rows of CameraUnlock.ini. Only the tracking mode pair and WorldSpaceYaw are Writable:
// the mode and yaw hotkeys save the player's choice, and End changes the session only.
cameraunlock::config::ConfigTable<Config> MakeConfigTable();

// ThiefHeadTracking.ini as the builds before the canonical format read it (legacy_config/),
// mapped into Config.
cameraunlock::config::LegacyImport<Config> MakeLegacyImport();

// The owner's options for the files in `folder` (with its trailing separator): the settings in
// CameraUnlock.ini, imported once from ThiefHeadTracking.ini. The mod passes
// DefaultsFile::PerUser() and a test a scratch file.
cameraunlock::config::ConfigOwnerOptions<Config> MakeConfigOwnerOptions(const std::wstring& folder,
                                                                        cameraunlock::config::DefaultsFile defaults);

}  // namespace ThiefHeadTracking
