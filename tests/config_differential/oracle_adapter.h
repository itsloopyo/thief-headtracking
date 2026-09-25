#pragma once

// The oracle: the config reader and hotkey registration of the newest published build (the
// rolling `dev` pre-release, adfccf8), compiled from oracle/ with the core sources they
// included at its pin (f4135c4). Two libraries build it, each with its namespaces renamed at
// compile time so it links beside the current core: the reader against the published
// HotkeyPoller, whose IsValidHotkeyCode it calls, and Hotkeys::Start against oracle_fake's
// recording poller. This header names no core type, so the test includes it without the
// renaming.

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace thief_oracle_view {

struct OracleConfig {
    bool enabled_on_startup;
    int udp_port;
    float sens_yaw, sens_pitch, sens_roll;
    bool invert_yaw, invert_pitch, invert_roll;
    float local_smoothing, remote_smoothing;
    bool move_crosshair;
    bool world_space_yaw;
    // cameraunlock::ads::AdsMode at f4135c4: 0 paused, 1 marker, 2 tracked.
    int ads_mode;
    bool position_enabled;
    float pos_sens_x, pos_sens_y, pos_sens_z;
    float pos_limit_x, pos_limit_y, pos_limit_z, pos_limit_z_back;
    float position_scale;
    bool collision_enabled;
    float collision_margin;
    float collision_release_smoothing;
    std::uint32_t collision_channel;
    bool struct_probe;
    int vk_toggle, vk_cycle_mode, vk_yaw_mode, vk_ads_mode;
    bool chord_toggle, chord_cycle_mode, chord_yaw_mode, chord_ads_mode;
};

struct OracleResult {
    // What Config::LoadOrCreate returned. false stopped the mod at startup.
    bool loaded;
    OracleConfig config;
};

// Config::LoadOrCreate on a default Config, as the published build's InitThreadBody ran it.
// Creates the file when there is none, as that build did.
OracleResult RunOracle(const std::string& path);

// The hotkey codes and chord switches the published build's Hotkeys::Start registered from.
struct HotkeyView {
    int vk_toggle, vk_cycle_mode, vk_yaw_mode;
    bool chord_toggle, chord_cycle_mode, chord_yaw_mode;
};

// Which actions a key press fires, for every key a binding can name (0x01-0xFE) under every
// set of held modifiers. Entry (vk - kFirstKey) * kHeldStates + held counts the toggle,
// cycle and yaw mode actions fired, in that order. held: 1 Ctrl, 2 Shift, 4 Alt.
constexpr int kFirstKey = 0x01;
constexpr int kLastKey = 0xFE;
constexpr int kHeldStates = 8;
using FireTable = std::vector<std::array<int, 3>>;

// The published build's Hotkeys::Start run on `keys`, pressing each key under each held set.
// The ADS action, which f74fcee removed, is registered unbound and not counted.
FireTable OracleFires(const HotkeyView& keys);

}
