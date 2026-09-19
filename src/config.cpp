// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "config.h"

#include "config_sanitize.h"
#include "logging.h"

#include "cameraunlock/config/ini_reader.h"
#include "cameraunlock/input/hotkey_poller.h"

#include <windows.h>

#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <string>

namespace ThiefHeadTracking {

namespace {

// Every default and bound this file writes and reads lives in config.h, so the writer,
// the reader's per-key fallback and the Config member initialisers cannot drift apart.
// The float-typed defaults widen to double for the WriteDouble calls and match ReadFloat
// exactly on the read side.
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
    w.WriteComment("What head tracking does while the bow is drawn:");
    w.WriteComment("  paused  - the game keeps the camera until you lower the bow (default)");
    w.WriteComment("  tracked - head tracking carries on, and the game's own crosshair keeps");
    w.WriteComment("            marking the aim point");
    w.WriteComment("Cycled in game with Insert or Ctrl+Shift+U, which writes the choice back here.");
    w.WriteString("AdsMode", cameraunlock::ads::AdsModeValue(kDefaultAdsMode));
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
    w.WriteComment("Virtual-key codes. Defaults: End (toggle), Page Up (cycle tracking mode), Page Down (yaw mode), Insert (cycle ADS mode).");
    w.WriteHex("Toggle", kDefaultVkToggle);
    w.WriteHex("CycleMode", kDefaultVkCycleMode);
    w.WriteHex("YawMode", kDefaultVkYawMode);
    w.WriteHex("AdsMode", kDefaultVkAdsMode);
    w.WriteComment("Chord alternatives: Ctrl+Shift+Y (toggle), Ctrl+Shift+G (cycle tracking mode), Ctrl+Shift+H (yaw mode), Ctrl+Shift+U (cycle ADS mode).");
    w.WriteBool("ChordToggle", kDefaultChord);
    w.WriteBool("ChordCycleMode", kDefaultChord);
    w.WriteBool("ChordYawMode", kDefaultChord);
    w.WriteBool("ChordAdsMode", kDefaultChord);
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

// The old value is deliberately NOT migrated into the new keys. The single
// Smoothing value carried a hidden 0.15 floor, so the number in an existing
// config does not mean what it used to: copying it across would hand a local
// user smoothing they never chose under the new semantics, and copying it into
// only one of the two keys would be a guess about which connection they were on.
void WarnRetiredSmoothingKey(const cameraunlock::IniReader& reader,
                             const char* section, const char* key) {
    if (reader.ReadString(section, key, "").empty()) return;
    Log::Line(
        "WARN: Config key [%s] %s has been retired and is IGNORED. Smoothing is now two "
        "keys: LocalSmoothing (default 0, applies to a tracker on this machine) and "
        "RemoteSmoothing (default 0.15, applies to a tracker on the network). The "
        "old value is not migrated because the semantics changed - it carried a "
        "hidden 0.15 floor that no longer exists. Set the two new keys.",
        section, key);
}

// Reports a value the sanitizer had to change, and returns the sanitized one, so a
// setting the mod is not honouring never passes silently.
float ReportIfSanitized(const char* section, const char* key, float raw, float clean) {
    if (raw != clean) {
        Log::Line("WARN: INI [%s] %s value %.4f out of range or non-finite; using %.4f",
                  section, key, raw, clean);
    }
    return clean;
}

// The raw text of a key, or an empty string when the key is absent.
//
// IniReader answers with the caller's default in BOTH cases - key absent, and key present
// holding something it could not parse - and the two need different treatment. Absent is
// the documented default and says nothing. Present-but-unusable is an edit the mod is not
// honouring while the file goes on advertising it, which is the silence every other
// rejected value in this file was written to break.
std::string RawValue(const cameraunlock::IniReader& ini, const char* section,
                     const char* key) {
    return ini.ReadString(section, key, "");
}

// Whether the key holds a number core's reader could actually use.
//
// Answered by reading it TWICE with different defaults rather than by parsing the text
// here. IniReader hands back the caller's default both when a key is absent and when the
// parse consumes nothing, so one read cannot tell those apart - and a second parser written
// here would answer a subtly different question, because core parses in a pinned "C" locale
// and a game is free to have moved the CRT's LC_NUMERIC somewhere with a comma decimal
// separator. Two reads agree only when a real value came back.
bool ParsesAsNumber(const cameraunlock::IniReader& ini, const char* section,
                    const char* key) {
    return ini.ReadFloat(section, key, 0.0f) == ini.ReadFloat(section, key, 1.0f);
}

bool ParsesAsHex(const cameraunlock::IniReader& ini, const char* section, const char* key) {
    return ini.ReadHex(section, key, 0) == ini.ReadHex(section, key, 1);
}

// One place for "the key is there and is not a number", so every float in this file reports
// it rather than only the three that had wrappers.
bool RejectUnparseableFloat(const cameraunlock::IniReader& ini, const char* section,
                            const char* key, float fallback) {
    const std::string text = RawValue(ini, section, key);
    if (text.empty() || ParsesAsNumber(ini, section, key)) {
        return false;
    }
    Log::Line("WARN: INI [%s] %s value '%s' is not a number; using %.4f",
              section, key, text.c_str(), fallback);
    return true;
}

// Whether ReadBool will recognise @p text.
//
// Core's exact strings, compared exactly, because core compares them exactly. Matching
// case-insensitively here would be the wider question and would wave through `fAlSe`, which
// core then fails to match and silently replaces with the default - the very silence this
// check exists to break, now with a validator saying the value was fine.
//
// The check is needed at all because ReadBool compares the WHOLE trimmed value:
// `Enabled=1 ; my comment` matches nothing and falls back with no diagnostic, and a
// trailing semicolon comment is the first thing a player types into an INI.
bool ParsesAsBool(const std::string& text) {
    static const char* const kTokens[] = {
        "1", "true", "True", "TRUE", "yes", "Yes", "YES", "on", "On", "ON",
        "0", "false", "False", "FALSE", "no", "No", "NO", "off", "Off", "OFF",
    };
    for (const char* token : kTokens) {
        if (text == token) {
            return true;
        }
    }
    return false;
}

bool ReadBoolChecked(const cameraunlock::IniReader& ini, const char* section,
                     const char* key, bool fallback) {
    const std::string raw = RawValue(ini, section, key);
    if (!raw.empty() && !ParsesAsBool(raw)) {
        Log::Line("WARN: INI [%s] %s value '%s' is not on or off; using %d. A trailing "
                  "'; comment' is read as part of the value, so keep comments on their "
                  "own line.", section, key, raw.c_str(), fallback ? 1 : 0);
        return fallback;
    }
    return ini.ReadBool(section, key, fallback);
}

// Reads a float that ends up multiplied into the injected viewpoint. strtod accepts
// "nan" and "inf", and either one propagates from here into the rotation or the camera
// location and leaves the player with a black screen and nothing in the log. Magnitude
// and sign are NOT checked: a sensitivity above 1 or a limit the player has widened are
// legitimate tuning.
float ReadFinite(const cameraunlock::IniReader& ini, const char* section, const char* key,
                 float fallback) {
    if (RejectUnparseableFloat(ini, section, key, fallback)) {
        return fallback;
    }
    const float raw = ini.ReadFloat(section, key, fallback);
    return ReportIfSanitized(section, key, raw, SanitizeFinite(raw, fallback));
}

// A position sensitivity, which is additionally required not to be negative. A negative
// one is an axis inversion wearing a sensitivity's clothes, and it lands ahead of the
// asymmetric z clamp - so it moves the generous forward budget onto the backward lean and
// reads in game as "leaning in barely moves, pulling back moves a lot". Axis direction is
// the tracker's to set, and the engine boundary owns the one conversion.
float ReadPositionSensitivity(const cameraunlock::IniReader& ini, const char* key,
                              float fallback) {
    const float value = ReadFinite(ini, "Position", key, fallback);
    if (value < 0.0f) {
        Log::Line("WARN: INI [Position] %s value %.4f is negative, which inverts the axis "
                  "before the lean limits are applied; using %.4f. Set the axis direction "
                  "in your tracker instead.", key, value, fallback);
        return fallback;
    }
    return value;
}

// A positional limit is additionally required to be above zero: a negative one inverts
// the processor's clamp and pins the camera at a constant offset. See config_sanitize.h.
float ReadLimit(const cameraunlock::IniReader& ini, const char* key, float fallback) {
    if (RejectUnparseableFloat(ini, "Position", key, fallback)) {
        return fallback;
    }
    const float raw = ini.ReadFloat("Position", key, fallback);
    return ReportIfSanitized("Position", key, raw, SanitizePositiveLimit(raw, fallback));
}

// Smoothing is additionally clamped to [0,1], the whole domain the settle speed is
// mapped from. See config_sanitize.h.
float ReadSmoothing(const cameraunlock::IniReader& ini, const char* key, float fallback) {
    if (RejectUnparseableFloat(ini, "Smoothing", key, fallback)) {
        return fallback;
    }
    const float raw = ini.ReadFloat("Smoothing", key, fallback);
    return ReportIfSanitized("Smoothing", key, raw, SanitizeSmoothing(raw, fallback));
}

// A 32-bit trace flag word, parsed here rather than through IniReader::ReadHex. See the
// call site for why: ReadHex is strtol into an int and saturates the top half of the range.
std::uint32_t ReadTraceFlags(const cameraunlock::IniReader& ini, std::uint32_t fallback) {
    const std::string text = RawValue(ini, "Collision", "Channel");
    if (text.empty()) {
        return fallback;
    }
    const char* begin = text.c_str();
    if (text.size() >= 2 && begin[0] == '0' && (begin[1] == 'x' || begin[1] == 'X')) {
        begin += 2;
    }
    // The sign is tested on the token itself, not searched for in the raw value. strtoul
    // accepts a leading '-' and wraps it, so it has to be refused - but the raw value
    // carries any trailing "; comment" with it, and scanning that for a hyphen rejected
    // `0x1000 ; camera-shake flags` as if the number were negative.
    char* end = nullptr;
    errno = 0;
    const unsigned long value = std::strtoul(begin, &end, 16);
    if (end == begin || errno == ERANGE || *begin == '-' || *begin == '+') {
        Log::Line("WARN: INI [Collision] Channel value '%s' is not a 32-bit trace flag "
                  "word; using the flags pinned for this build", text.c_str());
        return fallback;
    }
    return value != 0 ? static_cast<std::uint32_t>(value) : fallback;
}

// The INI holds the ADS mode as a string, so an unknown one has to land on the default
// rather than on whichever branch happens to be last. That covers a typo in a hand-edited
// file, and it is also the migration path for a mode renamed since an older release wrote
// the key: the player gets stock ADS rather than head tracking through their sights that
// they never asked for. `marker` is one of those - it is a three-slot mod's mode and this
// mod has no such slot, so it lands on the default too.
//
// Reported rather than silently corrected, because the file goes on advertising a mode the
// mod is not honouring.
AdsMode ReadAdsMode(const cameraunlock::IniReader& ini) {
    const std::string raw = ini.ReadString("General", "AdsMode", "");
    if (raw.empty()) {
        return kDefaultAdsMode;
    }
    const AdsMode mode = ParseAdsMode(raw.c_str());
    // ParseAdsMode trims and lowercases before matching, so comparing the trimmed input
    // against the value it resolved to is what tells a recognised spelling from a fallback.
    std::string trimmed = raw;
    const char* kSpace = " \t\r\n\v\f";
    const std::size_t first = trimmed.find_first_not_of(kSpace);
    if (first == std::string::npos) {
        return kDefaultAdsMode;
    }
    trimmed = trimmed.substr(first, trimmed.find_last_not_of(kSpace) - first + 1);
    if (_stricmp(trimmed.c_str(), cameraunlock::ads::AdsModeValue(mode)) != 0) {
        Log::Line("WARN: INI [General] AdsMode value '%s' is not one this mod has; using "
                  "'%s'. The two it takes are 'paused' and 'tracked'.",
                  trimmed.c_str(), cameraunlock::ads::AdsModeValue(mode));
    }
    return mode;
}

// Returns false on a port outside the bindable range, which is the one config error
// the mod refuses to start on: every other bad value has a usable fallback.
bool ReadGeneralSection(Config& cfg, const cameraunlock::IniReader& ini) {
    cfg.enabled_on_startup =
        ReadBoolChecked(ini, "General", "EnableOnStartup", kDefaultEnableOnStartup);
    // Parsed from the raw token rather than by reading the key twice, which is the trick
    // the float and hex checks use. It does not work here: IniReader::ReadInt is
    // GetPrivateProfileIntA on Windows, and that API converts the string itself and answers
    // 0 for anything unparseable - it never returns the default it was handed. Both reads
    // therefore agree on a typo, the check never fires, and `Port=abc` reaches the range
    // gate as 0 and is reported as a port out of range instead of as a typo.
    //
    // A local parse is safe for an integer specifically: LC_NUMERIC moves the decimal
    // separator, and there is not one in a port number.
    const std::string portText = RawValue(ini, "General", "Port");
    if (!portText.empty()) {
        const char* begin = portText.c_str();
        char* end = nullptr;
        std::strtol(begin, &end, 10);
        if (end == begin) {
            Log::Line("ERROR: INI [General] Port value '%s' is not a number",
                      portText.c_str());
            return false;
        }
    }
    const int port = ini.ReadInt("General", "Port", kDefaultPort);
    if (port < kMinPort || port > kMaxPort) {
        Log::Line("ERROR: INI port %d out of range %d-%d", port, kMinPort, kMaxPort);
        return false;
    }
    cfg.udp_port = static_cast<uint16_t>(port);
    cfg.world_space_yaw =
        ReadBoolChecked(ini, "General", "WorldSpaceYaw", kDefaultWorldSpaceYaw);
    cfg.move_crosshair =
        ReadBoolChecked(ini, "General", "MoveCrosshair", kDefaultMoveCrosshair);
    cfg.ads_mode = ReadAdsMode(ini);
    return true;
}

void ReadSensitivitySection(Config& cfg, const cameraunlock::IniReader& ini) {
    cfg.sens_yaw   = ReadFinite(ini, "Sensitivity", "Yaw",   kDefaultSensitivity);
    cfg.sens_pitch = ReadFinite(ini, "Sensitivity", "Pitch", kDefaultSensitivity);
    cfg.sens_roll  = ReadFinite(ini, "Sensitivity", "Roll",  kDefaultSensitivity);
    cfg.invert_yaw   = ReadBoolChecked(ini, "Sensitivity", "InvertYaw",   kDefaultInvert);
    cfg.invert_pitch = ReadBoolChecked(ini, "Sensitivity", "InvertPitch", kDefaultInvert);
    cfg.invert_roll  = ReadBoolChecked(ini, "Sensitivity", "InvertRoll",  kDefaultInvert);
}

void ReadSmoothingSection(Config& cfg, const cameraunlock::IniReader& ini) {
    cfg.local_smoothing  = ReadSmoothing(ini, "LocalSmoothing",  kDefaultLocalSmoothing);
    cfg.remote_smoothing = ReadSmoothing(ini, "RemoteSmoothing", kDefaultRemoteSmoothing);

    WarnRetiredSmoothingKey(ini, "Smoothing", "Smoothing");
    WarnRetiredSmoothingKey(ini, "Position", "Smoothing");
}

void ReadPositionSection(Config& cfg, const cameraunlock::IniReader& ini) {
    cfg.position_enabled =
        ReadBoolChecked(ini, "Position", "Enabled", kDefaultPositionEnabled);
    cfg.pos_sens_x = ReadPositionSensitivity(ini, "SensitivityX", kDefaultPosSens);
    cfg.pos_sens_y = ReadPositionSensitivity(ini, "SensitivityY", kDefaultPosSens);
    cfg.pos_sens_z = ReadPositionSensitivity(ini, "SensitivityZ", kDefaultPosSens);
    cfg.pos_limit_x = ReadLimit(ini, "LimitX", kDefaultPosLimitX);
    cfg.pos_limit_y = ReadLimit(ini, "LimitY", kDefaultPosLimitY);
    cfg.pos_limit_z = ReadLimit(ini, "LimitZ", kDefaultPosLimitZ);
    cfg.pos_limit_z_back = ReadLimit(ini, "LimitZBack", kDefaultPosLimitZBack);
    // PositionScale multiplies the clamped lean straight into the camera location, so a
    // non-finite one reaches the viewpoint even with every limit intact.
    cfg.position_scale = ReadFinite(ini, "Position", "PositionScale", kDefaultPositionScale);
}

void ReadCollisionSection(Config& cfg, const cameraunlock::IniReader& ini) {
    cfg.collision_enabled =
        ReadBoolChecked(ini, "Collision", "Enabled", kDefaultCollision);
    // A zero or negative margin puts the camera exactly on the surface it hit, where the
    // near clip plane renders straight through it - the clamp would then look broken
    // rather than absent. Sanitized like a positional limit for the same reason.
    cfg.collision_margin = kDefaultCollisionMargin;
    if (!RejectUnparseableFloat(ini, "Collision", "Margin", kDefaultCollisionMargin)) {
        const float raw = ini.ReadFloat("Collision", "Margin", kDefaultCollisionMargin);
        cfg.collision_margin =
            ReportIfSanitized("Collision", "Margin", raw,
                              SanitizePositiveLimit(raw, kDefaultCollisionMargin));
    }
    // Shares the [0,1] domain and the CalculateSmoothingFactor mapping with the two
    // tracker smoothing values, so it is validated the same way.
    cfg.collision_release_smoothing = kDefaultCollisionRelease;
    if (!RejectUnparseableFloat(ini, "Collision", "ReleaseSmoothing",
                                kDefaultCollisionRelease)) {
        const float rawRelease =
            ini.ReadFloat("Collision", "ReleaseSmoothing", kDefaultCollisionRelease);
        cfg.collision_release_smoothing =
            ReportIfSanitized("Collision", "ReleaseSmoothing", rawRelease,
                              SanitizeSmoothing(rawRelease, kDefaultCollisionRelease));
    }
    // Zero means "use the flags the build profile pinned", which is what every player
    // should be on. Anything else is an override, and it is parsed here rather than through
    // ReadHex because the destination is a 32-bit flag word and ReadHex is strtol into an
    // int: on this target long is 32 bits, so it saturates every value above 0x7FFFFFFF to
    // 0x7FFFFFFF. `Channel=0xFFFFFFFF` - the obvious thing to try when the log says the
    // trace never hits anything, which is what the INI comment invites - would silently
    // lose bit 31 and never trip the negative check either.
    cfg.collision_channel = ReadTraceFlags(ini, kDefaultCollisionChannel);
}

// A rebind the poller cannot act on is the worst kind of config error: the key simply
// never fires, and the log says the binding was accepted. So a rejected code is reported
// and the documented default stands.
//
// The accepted set is core's IsValidHotkeyCode, and it is an ALLOW list rather than the
// whole [1, 254] range GetAsyncKeyState will poll: the function keys, the number row,
// the letters, the numpad, the navigation cluster, Escape and Space, plus Pause, Print
// Screen, Num Lock and Scroll Lock. The WARN says so, because telling a player that a
// perfectly pollable key such as Tab or backtick "is not a usable virtual-key code" sends
// them looking for a fault in their keyboard rather than at the list.
int ReadVirtualKey(const cameraunlock::IniReader& ini, const char* key, int fallback) {
    const std::string text = RawValue(ini, "Hotkeys", key);
    if (!text.empty() && !ParsesAsHex(ini, "Hotkeys", key)) {
        Log::Line("WARN: INI [Hotkeys] %s value '%s' is not a hex number; using the "
                  "default 0x%02X", key, text.c_str(), fallback);
        return fallback;
    }
    const int raw = ini.ReadHex("Hotkeys", key, fallback);
    if (cameraunlock::input::IsValidHotkeyCode(raw)) {
        return raw;
    }
    Log::Line("WARN: INI [Hotkeys] %s value 0x%02X is not one of the keys this mod binds "
              "to; using the default 0x%02X. It takes a function key, a letter, a digit, "
              "a numpad key, the navigation cluster, Escape or Space.",
              key, raw, fallback);
    return fallback;
}

void ReadDiagnosticsSection(Config& cfg, const cameraunlock::IniReader& ini) {
    cfg.struct_probe =
        ReadBoolChecked(ini, "Diagnostics", "StructProbe", kDefaultStructProbe);
}

void ReadHotkeysSection(Config& cfg, const cameraunlock::IniReader& ini) {
    cfg.vk_toggle     = ReadVirtualKey(ini, "Toggle",    kDefaultVkToggle);
    cfg.vk_cycle_mode = ReadVirtualKey(ini, "CycleMode", kDefaultVkCycleMode);
    cfg.vk_yaw_mode   = ReadVirtualKey(ini, "YawMode",   kDefaultVkYawMode);
    cfg.vk_ads_mode   = ReadVirtualKey(ini, "AdsMode",   kDefaultVkAdsMode);
    cfg.chord_toggle     = ReadBoolChecked(ini, "Hotkeys", "ChordToggle",    kDefaultChord);
    cfg.chord_cycle_mode = ReadBoolChecked(ini, "Hotkeys", "ChordCycleMode", kDefaultChord);
    cfg.chord_yaw_mode   = ReadBoolChecked(ini, "Hotkeys", "ChordYawMode",   kDefaultChord);
    cfg.chord_ads_mode   = ReadBoolChecked(ini, "Hotkeys", "ChordAdsMode",   kDefaultChord);
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

    cameraunlock::IniReader ini;
    if (!ini.Open(iniPath)) {
        Log::Line("ERROR: Failed to open INI: %s", iniPath);
        return false;
    }

    if (!ReadGeneralSection(*this, ini)) {
        return false;
    }
    ReadSensitivitySection(*this, ini);
    ReadSmoothingSection(*this, ini);
    ReadPositionSection(*this, ini);
    ReadCollisionSection(*this, ini);
    ReadHotkeysSection(*this, ini);
    ReadDiagnosticsSection(*this, ini);
    return true;
}

// WritePrivateProfileString rather than a rewrite of the file: the INI carries the comments
// the player reads settings by, and only this one key is ours to change.
bool SaveAdsMode(const char* iniPath, AdsMode mode) {
    if (!iniPath || !*iniPath) {
        return false;
    }
    return WritePrivateProfileStringA("General", "AdsMode",
                                      cameraunlock::ads::AdsModeValue(mode), iniPath) != FALSE;
}

}  // namespace ThiefHeadTracking
