// The config differential test (convert-a-mod-to-the-canonical-config, section 5). Every input
// is read three ways:
//
//   oracle     the reader of the newest published build, the rolling `dev` pre-release at
//              adfccf8, with the core sources it compiled at its pin f4135c4 (oracle_adapter.h)
//   import     the frozen reader in src/legacy_config/
//   migration  the config owner in a folder holding only ThiefHeadTracking.ini, the legacy
//              file, importing it into a new CameraUnlock.ini, then the canonical reader and
//              table on that file
//
// Comparison 1, oracle against import, on every input: load status, every field both read
// (floats bit for bit), the startup state, and which actions every key press fires under every
// set of held modifiers. The differences it may find are kComparison1Differences below.
//
// Comparison 2, import against migration, is the proof for the migration: the settings the mod
// starts on and the actions every key press fires are the import's, apart from the approved
// changes, each of which the import must record as dropped. A sensitivity, inversion or unit
// scale the player set away from its shipped value is dropped (pose_shaping), MoveCrosshair=false
// is dropped (reticle: the game's crosshair always follows the aim now), and [Collision]
// Enabled, which shipped off pending verification, takes the table's default (follows_default).
// A value the canonical row cannot hold has no approved rule, so the owner defers that import
// and the session runs on what the import gave (kUnrepresentable).
//
// Comparison 2 runs twice, once over a Defaults.ini at the built-in values and once over one a
// player changed, since the migration writes default exactly where the imported value equals
// what Defaults.ini gives. After every load ThiefHeadTracking.ini keeps its bytes, its write
// time and its attributes, Defaults.ini is never written, and the folder holds the legacy file
// and CameraUnlock.ini and nothing else (the legacy file alone when nothing was imported). The
// next load reads CameraUnlock.ini, imports nothing and writes nothing, and a read-only legacy
// file imports as a writable one does.
//
// The distinct migrated files are written beside the executable under migrated\, for
// lint-migrated.mjs to run core's canonical config lint over.
//
// The key presses go through the published build's own Hotkeys::Start and the guards it
// compiled (OracleFires), and through the guard the current build registers (CurrentFires).
//
// Inputs: no file, an empty file, the dev build's first-run output (no build shipped a config
// or a launcher seed), and core's corpus over that output.

#include "config.h"
#include "legacy_config/legacy_config.h"
#include "oracle_adapter.h"
#include "ue3_math.h"

#include "cameraunlock/config/config_owner.h"
#include "cameraunlock/config/defaults_file.h"
#include "cameraunlock/config/testing/ini_mutations.h"
#include "cameraunlock/input/key_binding_registration.h"
#include "cameraunlock/input/key_bindings.h"
#include "cameraunlock/tracking/tracking_mode.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace ThiefHeadTracking;

namespace {

// Comparison 1's one difference, present in every input: the published build also read
// [General] AdsMode, [Hotkeys] AdsMode and [Hotkeys] ChordAdsMode, started in the ADS mode
// AdsMode named, registered the ADS cycle on Insert and Ctrl+Shift+U, and wrote AdsMode back
// to the file when it was cycled. f74fcee (Shooter ADS handling: remove the bow-aim mode
// cycle, keep tracking on through the draw) removed all of it before the conversion, so the
// import reads none of the three and registers no fourth action.
const char* const kComparison1Differences[] = {
    "[General] AdsMode, [Hotkeys] AdsMode, [Hotkeys] ChordAdsMode: read by dev (adfccf8), not read since f74fcee",
};

// The published build read the four [Position] limits with no upper bound, and the canonical
// rows take 0 to 10 metres. Core has no rule for a value outside a concept's range, so the owner
// defers such a file: it stays as it is, the session runs on what the import read, and nothing
// is saved.
const char* const kUnrepresentable =
    "[Position] LimitX, LimitY, LimitZ or LimitZBack above 10, which the canonical rows cannot hold, so the "
    "import defers";

constexpr float kMaxCanonicalLimit = 10.0f;

constexpr const char* kFileName = "ThiefHeadTracking.ini";

int g_failures = 0;
int g_checks = 0;

void Check(bool cond, const std::string& what) {
    ++g_checks;
    if (!cond) {
        if (g_failures < 200) std::printf("  FAIL: %s\n", what.c_str());
        ++g_failures;
    }
}

bool SameBits(float a, float b) { return std::memcmp(&a, &b, sizeof a) == 0; }

std::string ReadBytes(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void WriteBytes(const fs::path& path, const std::string& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!out) throw std::runtime_error("could not write " + path.string());
}

struct Listing {
    std::vector<std::pair<std::string, std::string>> files;
    bool operator==(const Listing& o) const { return files == o.files; }
};

Listing List(const fs::path& dir) {
    Listing l;
    for (const auto& e : fs::directory_iterator(dir)) {
        l.files.emplace_back(e.path().filename().string(), ReadBytes(e.path()));
    }
    std::sort(l.files.begin(), l.files.end());
    return l;
}

void SetReadOnly(const fs::path& path, bool readOnly) {
    const DWORD attrs = GetFileAttributesW(path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) throw std::runtime_error("no attributes for " + path.string());
    const DWORD next = readOnly ? (attrs | FILE_ATTRIBUTE_READONLY) : (attrs & ~FILE_ATTRIBUTE_READONLY);
    if (!SetFileAttributesW(path.c_str(), next)) throw std::runtime_error("could not set attributes on " + path.string());
}

struct Input {
    std::string name;
    std::optional<std::string> bytes;  // nullopt: no file
};

// Startup state as dev:src/tracking_runtime.cpp Start derives it from the config: enabled
// from enabled_on_startup, RotationAndPosition when position_enabled else RotationOnly, the
// yaw mode from world_space_yaw. The ADS mode it also started in is kComparison1Differences.
struct Startup {
    bool enabled;
    int mode;  // cameraunlock::TrackingMode: 0 rotation and position, 1 rotation only
    bool world_space_yaw;
};

Startup StartupOf(const thief_oracle_view::OracleConfig& c) {
    return {c.enabled_on_startup, c.position_enabled ? 0 : 1, c.world_space_yaw};
}

Startup StartupOf(const legacy::Config& c) {
    return {c.enabled_on_startup, c.position_enabled ? 0 : 1, c.world_space_yaw};
}

bool SameStartup(const Startup& a, const Startup& b) {
    return a.enabled == b.enabled && a.mode == b.mode && a.world_space_yaw == b.world_space_yaw;
}

thief_oracle_view::HotkeyView KeysOf(const thief_oracle_view::OracleConfig& c) {
    return {c.vk_toggle, c.vk_cycle_mode, c.vk_yaw_mode, c.chord_toggle, c.chord_cycle_mode, c.chord_yaw_mode};
}

thief_oracle_view::HotkeyView KeysOf(const legacy::Config& c) {
    return {c.vk_toggle, c.vk_cycle_mode, c.vk_yaw_mode, c.chord_toggle, c.chord_cycle_mode, c.chord_yaw_mode};
}

cameraunlock::input::KeyModifiers g_currentHeld = cameraunlock::input::KeyModifiers::kNone;

cameraunlock::input::KeyModifiers CurrentHeld() { return g_currentHeld; }

cameraunlock::input::KeyModifiers ModifiersOf(int held) {
    using cameraunlock::input::KeyModifiers;
    KeyModifiers m = KeyModifiers::kNone;
    if ((held & 1) != 0) m = m | KeyModifiers::kCtrl;
    if ((held & 2) != 0) m = m | KeyModifiers::kShift;
    if ((held & 4) != 0) m = m | KeyModifiers::kAlt;
    return m;
}

// OracleFires' table for the current build. Its Hotkeys::Start parses each key list and hands
// it to RegisterKeyBindings, which puts one detail::GuardKey callback per distinct key on the
// poller, holding that key's bindings in list order. The same callbacks are built here with
// the held modifiers read from the test rather than the keyboard, since the poller keeps its
// callbacks to itself.
thief_oracle_view::FireTable CurrentFires(const Config& m) {
    using thief_oracle_view::kFirstKey;
    using thief_oracle_view::kHeldStates;
    using thief_oracle_view::kLastKey;
    std::array<int, 3> fired{};
    std::vector<std::pair<int, std::function<void()>>> registered;
    const std::string* lists[3] = {&m.toggle_key_name, &m.cycle_tracking_mode_key_name, &m.yaw_mode_key_name};
    for (int action = 0; action < 3; ++action) {
        const cameraunlock::input::KeyBindingsParseResult parsed = cameraunlock::input::ParseKeyBindings(*lists[action]);
        if (!parsed.ok()) throw std::logic_error("migrated hotkey list '" + *lists[action] + "' does not parse");
        std::vector<int> keys;
        std::vector<std::vector<cameraunlock::input::KeyModifiers>> modifiers;
        for (const cameraunlock::input::KeyBinding& b : parsed.bindings) {
            const auto at = std::find(keys.begin(), keys.end(), b.vk);
            if (at == keys.end()) {
                keys.push_back(b.vk);
                modifiers.push_back({b.modifiers});
            } else {
                modifiers[static_cast<std::size_t>(at - keys.begin())].push_back(b.modifiers);
            }
        }
        for (std::size_t i = 0; i < keys.size(); ++i) {
            registered.emplace_back(keys[i], cameraunlock::input::detail::GuardKey(
                                                 std::move(modifiers[i]), [&fired, action] { ++fired[action]; },
                                                 &CurrentHeld));
        }
    }

    thief_oracle_view::FireTable table;
    table.reserve((kLastKey - kFirstKey + 1) * kHeldStates);
    for (int vk = kFirstKey; vk <= kLastKey; ++vk) {
        for (int held = 0; held < kHeldStates; ++held) {
            fired = {};
            g_currentHeld = ModifiersOf(held);
            for (const auto& r : registered) {
                if (r.first == vk) r.second();
            }
            table.push_back(fired);
        }
    }
    g_currentHeld = cameraunlock::input::KeyModifiers::kNone;
    return table;
}

// The first press whose fired actions differ, for the failure message.
std::string FirstFireDifference(const thief_oracle_view::FireTable& expected, const thief_oracle_view::FireTable& got) {
    for (std::size_t i = 0; i < expected.size() && i < got.size(); ++i) {
        if (expected[i] != got[i]) {
            char text[160];
            std::snprintf(text, sizeof text, "key 0x%02X held %d fires %d/%d/%d, not %d/%d/%d",
                          static_cast<int>(i / thief_oracle_view::kHeldStates) + thief_oracle_view::kFirstKey,
                          static_cast<int>(i % thief_oracle_view::kHeldStates), got[i][0], got[i][1], got[i][2],
                          expected[i][0], expected[i][1], expected[i][2]);
            return text;
        }
    }
    return expected.size() == got.size() ? "none" : "the tables differ in size";
}

// Every field the import reads, against the oracle's field of the same name.
std::vector<std::string> FieldDifferences(const thief_oracle_view::OracleConfig& o, const legacy::Config& i) {
    std::vector<std::string> d;
    auto b = [&d](const char* n, bool x, bool y) { if (x != y) d.push_back(n); };
    auto f = [&d](const char* n, float x, float y) { if (!SameBits(x, y)) d.push_back(n); };
    auto n = [&d](const char* name, long long x, long long y) { if (x != y) d.push_back(name); };
    b("enabled_on_startup", o.enabled_on_startup, i.enabled_on_startup);
    n("udp_port", o.udp_port, i.udp_port);
    f("sens_yaw", o.sens_yaw, i.sens_yaw);
    f("sens_pitch", o.sens_pitch, i.sens_pitch);
    f("sens_roll", o.sens_roll, i.sens_roll);
    b("invert_yaw", o.invert_yaw, i.invert_yaw);
    b("invert_pitch", o.invert_pitch, i.invert_pitch);
    b("invert_roll", o.invert_roll, i.invert_roll);
    f("local_smoothing", o.local_smoothing, i.local_smoothing);
    f("remote_smoothing", o.remote_smoothing, i.remote_smoothing);
    b("move_crosshair", o.move_crosshair, i.move_crosshair);
    b("world_space_yaw", o.world_space_yaw, i.world_space_yaw);
    b("position_enabled", o.position_enabled, i.position_enabled);
    f("pos_sens_x", o.pos_sens_x, i.pos_sens_x);
    f("pos_sens_y", o.pos_sens_y, i.pos_sens_y);
    f("pos_sens_z", o.pos_sens_z, i.pos_sens_z);
    f("pos_limit_x", o.pos_limit_x, i.pos_limit_x);
    f("pos_limit_y", o.pos_limit_y, i.pos_limit_y);
    f("pos_limit_z", o.pos_limit_z, i.pos_limit_z);
    f("pos_limit_z_back", o.pos_limit_z_back, i.pos_limit_z_back);
    f("position_scale", o.position_scale, i.position_scale);
    b("collision_enabled", o.collision_enabled, i.collision_enabled);
    f("collision_margin", o.collision_margin, i.collision_margin);
    f("collision_release_smoothing", o.collision_release_smoothing, i.collision_release_smoothing);
    n("collision_channel", o.collision_channel, i.collision_channel);
    b("struct_probe", o.struct_probe, i.struct_probe);
    n("vk_toggle", o.vk_toggle, i.vk_toggle);
    n("vk_cycle_mode", o.vk_cycle_mode, i.vk_cycle_mode);
    n("vk_yaw_mode", o.vk_yaw_mode, i.vk_yaw_mode);
    b("chord_toggle", o.chord_toggle, i.chord_toggle);
    b("chord_cycle_mode", o.chord_cycle_mode, i.chord_cycle_mode);
    b("chord_yaw_mode", o.chord_yaw_mode, i.chord_yaw_mode);
    return d;
}

std::string Join(const std::vector<std::string>& v) {
    std::string s;
    for (const std::string& x : v) s += (s.empty() ? "" : ", ") + x;
    return s;
}

// The corpus descriptor of every key the frozen reader reads.
std::vector<cameraunlock::config::testing::MutationKey> MutationKeys() {
    using cameraunlock::config::testing::ChordSwitch;
    using cameraunlock::config::testing::MutationKey;
    auto plain = [](const char* s, const char* k, const char* alt, std::vector<std::string> oor = {}) {
        MutationKey m;
        m.section = s;
        m.key = k;
        m.alternate = alt;
        m.out_of_range = std::move(oor);
        return m;
    };
    auto hotkey = [](const char* k, const char* alt, const char* chord) {
        MutationKey m;
        m.section = "Hotkeys";
        m.key = k;
        m.alternate = alt;
        m.out_of_range = {"0x07"};
        m.hotkey = true;
        m.chords.push_back(ChordSwitch{"Hotkeys", chord, "true", "false"});
        return m;
    };
    return {
        plain("General", "EnableOnStartup", "false"),
        plain("General", "Port", "4243", {"1023", "65536"}),
        plain("General", "WorldSpaceYaw", "false"),
        plain("General", "MoveCrosshair", "false"),
        plain("Sensitivity", "Yaw", "0.5"),
        plain("Sensitivity", "Pitch", "0.5"),
        plain("Sensitivity", "Roll", "0.5"),
        plain("Sensitivity", "InvertYaw", "true"),
        plain("Sensitivity", "InvertPitch", "true"),
        plain("Sensitivity", "InvertRoll", "true"),
        plain("Smoothing", "LocalSmoothing", "0.3", {"-0.5", "1.5"}),
        plain("Smoothing", "RemoteSmoothing", "0.3", {"-0.5", "1.5"}),
        plain("Smoothing", "Smoothing", "0.3"),
        plain("Position", "Smoothing", "0.3"),
        plain("Position", "Enabled", "false"),
        plain("Position", "SensitivityX", "0.5", {"-1.0"}),
        plain("Position", "SensitivityY", "0.5", {"-1.0"}),
        plain("Position", "SensitivityZ", "0.5", {"-1.0"}),
        plain("Position", "LimitX", "0.5", {"-0.3"}),
        plain("Position", "LimitY", "0.5", {"-0.2"}),
        plain("Position", "LimitZ", "0.5", {"-0.4"}),
        plain("Position", "LimitZBack", "0.2", {"-0.1"}),
        plain("Position", "PositionScale", "50"),
        plain("Collision", "Enabled", "true"),
        plain("Collision", "Margin", "30", {"-20"}),
        plain("Collision", "ReleaseSmoothing", "0.5", {"-0.5", "1.5"}),
        plain("Collision", "Channel", "0x2086", {"0x100000000"}),
        hotkey("Toggle", "0x70", "ChordToggle"),
        hotkey("CycleMode", "0x71", "ChordCycleMode"),
        hotkey("YawMode", "0x72", "ChordYawMode"),
        plain("Hotkeys", "ChordToggle", "false"),
        plain("Hotkeys", "ChordCycleMode", "false"),
        plain("Hotkeys", "ChordYawMode", "false"),
        plain("Diagnostics", "StructProbe", "true"),
    };
}

// One folder per reading under a root of this process's own, emptied before each input so the
// test never holds more than one input's files.
class Scratch {
public:
    Scratch() {
        root_ = fs::temp_directory_path() / ("thief-config-differential-" + std::to_string(GetCurrentProcessId()));
        Remove(root_);
        fs::create_directories(root_);
    }
    ~Scratch() { Remove(root_); }
    Scratch(const Scratch&) = delete;
    Scratch& operator=(const Scratch&) = delete;

    // root/leaf, created empty.
    fs::path Clean(const std::string& leaf) {
        const fs::path dir = root_ / leaf;
        Remove(dir);
        fs::create_directories(dir);
        return dir;
    }
    const fs::path& Root() const { return root_; }

private:
    // Read-only files included, which remove_all will not delete.
    static void Remove(const fs::path& dir) {
        std::error_code ec;
        if (!fs::exists(dir, ec)) return;
        for (const auto& e : fs::recursive_directory_iterator(dir, ec)) {
            if (e.is_regular_file()) SetFileAttributesW(e.path().c_str(), FILE_ATTRIBUTE_NORMAL);
        }
        fs::remove_all(dir, ec);
        if (ec) throw std::runtime_error("could not empty " + dir.string() + ": " + ec.message());
    }

    fs::path root_;
};

fs::path Place(const fs::path& dir, const Input& input) {
    const fs::path file = dir / kFileName;
    if (input.bytes) WriteBytes(file, *input.bytes);
    return file;
}

struct ImportRun {
    legacy::Config config;
    legacy::ReadResult result;
};

// The import on a read-only copy of the input, which must leave its folder as it found it.
ImportRun RunImport(Scratch& scratch, const Input& input) {
    const fs::path dir = scratch.Clean("import");
    const fs::path file = Place(dir, input);
    if (input.bytes) SetReadOnly(file, true);
    const Listing before = List(dir);
    ImportRun run;
    run.result = run.config.Read(file.string().c_str());
    Check(List(dir) == before, input.name + ": the import changed its folder");
    return run;
}

ImportRun Comparison1(Scratch& scratch, const Input& input) {
    const fs::path odir = scratch.Clean("oracle");
    const thief_oracle_view::OracleResult oracle = thief_oracle_view::RunOracle(Place(odir, input).string());
    const ImportRun import = RunImport(scratch, input);

    const bool importUsable = import.result.status != legacy::ReadStatus::Refused;
    Check(oracle.loaded == importUsable, input.name + ": load status differs (oracle " +
                                             (oracle.loaded ? "loaded" : "refused") + ")");
    Check(input.bytes.has_value() || import.result.status == legacy::ReadStatus::Absent,
          input.name + ": no file is not Absent");
    if (oracle.loaded && importUsable) {
        const std::vector<std::string> fields = FieldDifferences(oracle.config, import.config);
        Check(fields.empty(), input.name + ": fields differ: " + Join(fields));
        Check(SameStartup(StartupOf(oracle.config), StartupOf(import.config)), input.name + ": startup state differs");
        const thief_oracle_view::FireTable oracleFires = thief_oracle_view::OracleFires(KeysOf(oracle.config));
        const thief_oracle_view::FireTable importFires = thief_oracle_view::OracleFires(KeysOf(import.config));
        Check(oracleFires == importFires,
              input.name + ": hotkeys fire differently: " + FirstFireDifference(oracleFires, importFires));
    }
    return import;
}

// ---------------------------------------------------------------------------
// Comparison 2
// ---------------------------------------------------------------------------

namespace cfg = cameraunlock::config;
using cfg::ConfigLoadStatus;
using cfg::DropRule;
using cfg::DroppedValue;
using cfg::ImportResult;
using cfg::ImportStatus;

// A file as the test holds it to: its bytes, its last write time and its attributes.
struct FileStamp {
    std::string bytes;
    FILETIME written{};
    DWORD attributes = 0;

    bool operator==(const FileStamp& o) const {
        return bytes == o.bytes && CompareFileTime(&written, &o.written) == 0 && attributes == o.attributes;
    }
    bool operator!=(const FileStamp& o) const { return !(*this == o); }
};

FileStamp Stamp(const fs::path& path) {
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) {
        throw std::runtime_error("cannot stat " + path.string());
    }
    FileStamp s;
    s.bytes = ReadBytes(path);
    s.written = data.ftLastWriteTime;
    s.attributes = data.dwFileAttributes;
    return s;
}

// Where Defaults.ini is for each run of comparison 2: at the built-in values, which the first
// load creates, and with the values a player changed, written from it.
fs::path g_builtinDefaults;
fs::path g_alteredDefaults;

cfg::ConfigOwnerOptions<Config> OwnerOptions(const fs::path& dir, const fs::path& defaults) {
    return MakeConfigOwnerOptions(dir.wstring() + L"\\", cfg::DefaultsFile::At(defaults.wstring()));
}

// The import with its map, for the values it records.
ImportResult RunMappedImport(Scratch& scratch, const Input& input) {
    const fs::path file = Place(scratch.Clean("mapped"), input);
    Config out = MakeConfigTable().defaults();
    return MakeLegacyImport().run(cfg::LegacyInput{file.wstring(), file.string(), false}, out);
}

const DroppedValue* FindDrop(const std::vector<DroppedValue>& dropped, DropRule rule, const char* section,
                             const char* key) {
    for (const DroppedValue& d : dropped) {
        if (d.rule == rule && d.section == section && d.key == key) return &d;
    }
    return nullptr;
}

struct Tally {
    std::string committed;
    std::set<std::string> migrated;
    struct Run {
        int created = 0;
        int imported = 0;
        int deferred = 0;
        int refused = 0;
        // Migrated files holding at least one default row.
        int with_default_rows = 0;
        // Migrated files holding a value on at least one row, which the committed file never does.
        int with_values = 0;
    } builtin, altered;
    int with_pose_shaping_dropped = 0;
    int with_reticle_dropped = 0;
    int with_follows_default = 0;
};

// Every pose-shaping value the frozen reader read is listed in its place, folded where it holds
// the value the published build shipped and dropped as PoseShaping where it does not; MoveCrosshair
// is dropped exactly where it was false, [Collision] Enabled exactly where it differs from the
// table's default, and nothing is dropped by any other rule.
void CheckDrops(const std::string& name, const legacy::Config& l, const ImportResult& imported, Tally& tally) {
    const legacy::Config shipped;
    struct Read {
        const char* section;
        const char* key;
        bool atShipped;
    };
    const Read reads[] = {
        {"Sensitivity", "Yaw", SameBits(l.sens_yaw, shipped.sens_yaw)},
        {"Sensitivity", "Pitch", SameBits(l.sens_pitch, shipped.sens_pitch)},
        {"Sensitivity", "Roll", SameBits(l.sens_roll, shipped.sens_roll)},
        {"Sensitivity", "InvertYaw", l.invert_yaw == shipped.invert_yaw},
        {"Sensitivity", "InvertPitch", l.invert_pitch == shipped.invert_pitch},
        {"Sensitivity", "InvertRoll", l.invert_roll == shipped.invert_roll},
        {"Position", "SensitivityX", SameBits(l.pos_sens_x, shipped.pos_sens_x)},
        {"Position", "SensitivityY", SameBits(l.pos_sens_y, shipped.pos_sens_y)},
        {"Position", "SensitivityZ", SameBits(l.pos_sens_z, shipped.pos_sens_z)},
        {"Position", "PositionScale", SameBits(l.position_scale, kWorldUnitsPerMetre)},
    };
    Check(imported.pose_shaping.size() == std::size(reads),
          name + ": the import lists " + std::to_string(imported.pose_shaping.size()) + " pose-shaping values, not 10");
    if (imported.pose_shaping.size() != std::size(reads)) return;
    bool anyDropped = false;
    for (size_t k = 0; k < std::size(reads); ++k) {
        const cfg::PoseShapingValue& v = imported.pose_shaping[k];
        const std::string label = std::string("[") + reads[k].section + "] " + reads[k].key;
        Check(v.section == reads[k].section && v.key == reads[k].key, name + ": " + label + " is not listed in its place");
        Check(v.folded == reads[k].atShipped, name + ": " + label + " is " + (v.folded ? "folded" : "dropped") + " wrongly");
        const bool listed = FindDrop(imported.dropped, DropRule::PoseShaping, reads[k].section, reads[k].key) != nullptr;
        Check(listed != reads[k].atShipped,
              name + ": " + label + (listed ? " is dropped at its shipped value" : " is changed and not dropped"));
        if (!reads[k].atShipped) anyDropped = true;
    }
    if (anyDropped) ++tally.with_pose_shaping_dropped;

    const bool reticle = FindDrop(imported.dropped, DropRule::Reticle, "General", "MoveCrosshair") != nullptr;
    Check(reticle == !l.move_crosshair, name + ": MoveCrosshair dropped does not match its value");
    if (reticle) ++tally.with_reticle_dropped;

    const bool follows = FindDrop(imported.dropped, DropRule::FollowsDefault, "Collision", "Enabled") != nullptr;
    Check(follows == (l.collision_enabled != MakeConfigTable().defaults().collision_enabled),
          name + ": [Collision] Enabled dropped does not match its value");
    if (follows) ++tally.with_follows_default;

    for (const DroppedValue& d : imported.dropped) {
        Check(d.rule == DropRule::PoseShaping || d.rule == DropRule::Reticle || d.rule == DropRule::FollowsDefault,
              name + ": the import drops [" + d.section + "] " + d.key + " by a rule this map never applies");
    }
}

// The settings the mod starts on after the migration against the ones the frozen reader's build
// started on, with the approved changes applied: identity pose shaping (CheckDrops holds the
// import to recording every value it leaves out), no reticle switch, and the lean clamp at the
// table's default. The frozen reader's build applied its one LimitY both ways.
std::vector<std::string> StartupDifferences(const legacy::Config& l, const Config& m) {
    std::vector<std::string> d;
    if (m.enable_on_startup != l.enabled_on_startup) d.push_back("EnableOnStartup");
    if (m.udp_port != l.udp_port) d.push_back("UdpPort");
    if (m.world_space_yaw != l.world_space_yaw) d.push_back("WorldSpaceYaw");
    const auto mode = cameraunlock::DecodeTrackingMode(m.rotation_enabled, m.position_enabled);
    if (!mode || *mode != (l.position_enabled ? cameraunlock::TrackingMode::RotationAndPosition
                                              : cameraunlock::TrackingMode::RotationOnly)) {
        d.push_back("tracking mode");
    }
    if (!SameBits(m.local_smoothing, l.local_smoothing) || !SameBits(m.position.local_smoothing, l.local_smoothing)) {
        d.push_back("LocalSmoothing");
    }
    if (!SameBits(m.remote_smoothing, l.remote_smoothing) || !SameBits(m.position.remote_smoothing, l.remote_smoothing)) {
        d.push_back("RemoteSmoothing");
    }
    if (!SameBits(m.position.limit_x, l.pos_limit_x)) d.push_back("PositionLimitX");
    if (!SameBits(m.position.limit_y, l.pos_limit_y)) d.push_back("PositionLimitY");
    if (!SameBits(m.position.limit_y_down, l.pos_limit_y)) d.push_back("PositionLimitYDown");
    if (!SameBits(m.position.limit_z, l.pos_limit_z)) d.push_back("PositionLimitZ");
    if (!SameBits(m.position.limit_z_back, l.pos_limit_z_back)) d.push_back("PositionLimitZBack");
    if (m.collision_enabled != MakeConfigTable().defaults().collision_enabled) d.push_back("CollisionEnabled");
    if (!SameBits(m.lean_clamp.skin, l.collision_margin)) d.push_back("CollisionMargin");
    if (!SameBits(m.lean_clamp.release_smoothing, l.collision_release_smoothing)) d.push_back("CollisionReleaseSmoothing");
    if (static_cast<std::uint32_t>(m.collision_channel) != l.collision_channel) d.push_back("CollisionChannel");
    if (m.struct_probe != l.struct_probe) d.push_back("StructProbe");
    const thief_oracle_view::FireTable before = thief_oracle_view::OracleFires(KeysOf(l));
    const thief_oracle_view::FireTable after = CurrentFires(m);
    if (before != after) d.push_back("hotkeys: " + FirstFireDifference(before, after));
    return d;
}

bool Unrepresentable(const legacy::Config& l) {
    return l.pos_limit_x > kMaxCanonicalLimit || l.pos_limit_y > kMaxCanonicalLimit ||
           l.pos_limit_z > kMaxCanonicalLimit || l.pos_limit_z_back > kMaxCanonicalLimit;
}

// Every field the table binds, as the canonical renderer writes it, so two Configs compare whole.
std::string AllValues(const Config& c) {
    return cfg::RenderCanonical(MakeConfigTable(), c, {kConfigDisplayName});
}

bool Contains(const std::vector<std::string>& lines, const std::string& text) {
    for (const std::string& line : lines) {
        if (line.find(text) != std::string::npos) return true;
    }
    return false;
}

using Files = std::vector<std::pair<std::string, std::string>>;

void Comparison2(Scratch& scratch, const Input& input, const ImportRun& import, const ImportResult* mapped,
                 const fs::path& defaults, Tally& tally) {
    const bool builtin = defaults == g_builtinDefaults;
    Tally::Run& run = builtin ? tally.builtin : tally.altered;
    const std::string name =
        input.name + (builtin ? " (Defaults.ini at the built-in values)" : " (Defaults.ini changed)");

    const fs::path dir = scratch.Clean("migration");
    const fs::path config = dir / kConfigFileName;
    const fs::path legacyFile = Place(dir, input);
    const FileStamp defaultsBefore = Stamp(defaults);
    FileStamp legacyBefore;
    if (input.bytes) legacyBefore = Stamp(legacyFile);

    const cfg::ConfigLoadResult<Config> loaded = cfg::ConfigOwner<Config>(OwnerOptions(dir, defaults)).Load();
    const Listing after = List(dir);
    Check(Stamp(defaults) == defaultsBefore, name + ": the load wrote Defaults.ini");
    if (input.bytes) {
        Check(Stamp(legacyFile) == legacyBefore, name + ": ThiefHeadTracking.ini did not keep its bytes, write time and attributes");
    }

    if (!input.bytes) {
        // A fresh install, which follows Defaults.ini.
        ++run.created;
        Check(loaded.status == ConfigLoadStatus::Created, name + ": no file is not Created");
        Check(after.files == Files{{kConfigFileName, tally.committed}},
              name + ": the folder does not hold CameraUnlock.ini as config/ThiefHeadTracking.ini and nothing else");
        if (builtin) {
            const std::vector<std::string> d = StartupDifferences(import.config, loaded.config);
            Check(d.empty(), name + ": comparison 2: " + Join(d));
        }
        return;
    }
    if (import.result.status == legacy::ReadStatus::Refused) {
        ++run.refused;
        Check(loaded.status == ConfigLoadStatus::LegacyRefused, name + ": a file the import refuses is not LegacyRefused");
        Check(after.files == Files{{kFileName, *input.bytes}}, name + ": a refused file got a CameraUnlock.ini or another file beside it");
        return;
    }

    if (builtin) CheckDrops(name, import.config, *mapped, tally);

    // Imported or deferred, the session runs on the settings the load hands back.
    {
        const std::vector<std::string> d = StartupDifferences(import.config, loaded.config);
        Check(d.empty(), name + ": comparison 2: " + Join(d));
    }

    if (Unrepresentable(import.config)) {
        ++run.deferred;
        Check(loaded.status == ConfigLoadStatus::Deferred,
              name + ": " + kUnrepresentable + ", but the load is " + cfg::ConfigLoadStatusName(loaded.status));
        Check(after.files == Files{{kFileName, *input.bytes}}, name + ": a deferred import created CameraUnlock.ini or another file");
        Check(loaded.reason.find("cannot be converted") != std::string::npos,
              name + ": the player is not told which value stops the import: " + loaded.reason);
        return;
    }

    ++run.imported;
    Check(loaded.status == ConfigLoadStatus::Migrated,
          name + ": the migration is " + cfg::ConfigLoadStatusName(loaded.status) + ": " + loaded.reason);
    if (loaded.status != ConfigLoadStatus::Migrated) return;
    Check(after.files.size() == 2 && after.files[0].first == kConfigFileName && after.files[1].first == kFileName &&
              after.files[1].second == *input.bytes,
          name + ": the folder does not hold ThiefHeadTracking.ini and CameraUnlock.ini and nothing else");
    Check(Contains(loaded.log, "created from"), name + ": the log does not say where CameraUnlock.ini came from");
    const std::string migrated = ReadBytes(config);
    tally.migrated.insert(migrated);
    if (migrated.find("=default\r\n") != std::string::npos) ++run.with_default_rows;
    if (migrated != tally.committed) ++run.with_values;

    // The next launch reads CameraUnlock.ini over the same Defaults.ini, with nothing to report,
    // to the same settings, does not import, and writes neither file.
    {
        const cfg::ConfigLoadResult<Config> reread = cfg::ConfigOwner<Config>(OwnerOptions(dir, defaults)).Load();
        Check(reread.status == ConfigLoadStatus::Canonical && reread.diagnostics.empty(),
              name + ": the next launch does not read CameraUnlock.ini cleanly");
        Check(AllValues(reread.config) == AllValues(loaded.config), name + ": the next launch runs on other settings");
        Check(!Contains(reread.log, "created from"), name + ": the next launch imports again");
        Check(Contains(reread.log, "is left as it was and is not read"),
              name + ": the next launch does not say ThiefHeadTracking.ini is not read");
        Check(List(dir) == after && Stamp(legacyFile) == legacyBefore && Stamp(defaults) == defaultsBefore,
              name + ": the next launch changed a file");
    }

    // A read-only ThiefHeadTracking.ini imports as a writable one does and keeps its attribute,
    // bytes and write time.
    if (builtin) {
        const fs::path roDir = scratch.Clean("read-only");
        const fs::path roLegacy = Place(roDir, input);
        SetReadOnly(roLegacy, true);
        const FileStamp roBefore = Stamp(roLegacy);
        const cfg::ConfigLoadResult<Config> fromReadOnly = cfg::ConfigOwner<Config>(OwnerOptions(roDir, defaults)).Load();
        Check(fromReadOnly.status == ConfigLoadStatus::Migrated && AllValues(fromReadOnly.config) == AllValues(loaded.config) &&
                  ReadBytes(roDir / kConfigFileName) == migrated,
              name + ": a read-only ThiefHeadTracking.ini does not import as a writable one does");
        Check(Stamp(roLegacy) == roBefore && (roBefore.attributes & FILE_ATTRIBUTE_READONLY) != 0,
              name + ": a read-only ThiefHeadTracking.ini did not keep its attribute, bytes and write time");
    }
}

// Defaults.ini as a player may have changed it, from the one the owner created: every value this
// game takes from it differs from the built-in one, each set to the corpus's alternate for the
// legacy key it comes from, so a corpus input holding that alternate migrates as default.
void WriteAlteredDefaults() {
    std::string text = ReadBytes(g_builtinDefaults);
    const std::pair<const char*, const char*> changes[] = {
        {"UdpPort=4242", "UdpPort=4243"},
        {"EnableOnStartup=true", "EnableOnStartup=false"},
        {"WorldSpaceYaw=true", "WorldSpaceYaw=false"},
        {"PositionEnabled=true", "PositionEnabled=false"},
        {"LocalSmoothing=0.0", "LocalSmoothing=0.3"},
        {"RemoteSmoothing=0.15", "RemoteSmoothing=0.3"},
        {"PositionLimitX=0.3", "PositionLimitX=0.5"},
        {"PositionLimitY=0.2", "PositionLimitY=0.5"},
        {"PositionLimitYDown=0.2", "PositionLimitYDown=0.5"},
        {"PositionLimitZ=0.4", "PositionLimitZ=0.5"},
        {"PositionLimitZBack=0.1", "PositionLimitZBack=0.2"},
        {"CollisionEnabled=true", "CollisionEnabled=false"},
        {"CollisionReleaseSmoothing=0.9", "CollisionReleaseSmoothing=0.5"},
        {"ToggleKey=End, Ctrl+Shift+Y", "ToggleKey=F1, Ctrl+Shift+Y"},
        {"CycleTrackingModeKey=PageUp, Ctrl+Shift+G", "CycleTrackingModeKey=F2, Ctrl+Shift+G"},
        {"YawModeKey=PageDown, Ctrl+Shift+H", "YawModeKey=F3, Ctrl+Shift+H"},
    };
    for (const auto& [from, to] : changes) {
        const std::string line = std::string("\r\n") + from + "\r\n";
        const size_t at = text.find(line);
        if (at == std::string::npos) throw std::runtime_error(std::string("the created Defaults.ini has no line ") + from);
        text.replace(at + 2, std::strlen(from), to);
    }
    fs::create_directories(g_alteredDefaults.parent_path());
    WriteBytes(g_alteredDefaults, text);
}

std::vector<Input> Inputs(const std::string& firstRun) {
    using cameraunlock::config::testing::GenerateIniMutations;
    std::vector<Input> inputs;
    inputs.push_back({"no file", std::nullopt});
    inputs.push_back({"empty file", std::string()});
    inputs.push_back({"dev first-run output", firstRun});
    for (auto& m : GenerateIniMutations(firstRun, legacy::ReadKeys(), MutationKeys())) {
        inputs.push_back({"corpus: " + m.name, std::move(m.bytes)});
    }
    return inputs;
}

}  // namespace

int main() {
    try {
        Scratch scratch;
        Tally tally;
        tally.committed = ReadBytes(fs::path(THIEF_COMMITTED_CONFIG));
        Check(!tally.committed.empty(), "config/ThiefHeadTracking.ini is missing");

        // Each Defaults.ini sits outside the game folder, in a user folder of its own whose
        // parent exists, as the owner requires before it creates the file.
        g_builtinDefaults = scratch.Clean("user-builtin") / "CameraUnlock" / "Defaults.ini";
        g_alteredDefaults = scratch.Clean("user-altered") / "CameraUnlock" / "Defaults.ini";
        {
            const fs::path dir = scratch.Clean("first-load");
            Check(cfg::ConfigOwner<Config>(OwnerOptions(dir, g_builtinDefaults)).Load().status == ConfigLoadStatus::Created,
                  "the first load is not Created");
            Check(fs::exists(g_builtinDefaults), "the first load did not create Defaults.ini");
        }
        WriteAlteredDefaults();

        // The dev build's first-run output, committed once as test data, is what the oracle
        // still writes for a missing file.
        const std::string firstRun = ReadBytes(fs::path(THIEF_DIFFERENTIAL_DATA) / "dev-first-run.ini");
        Check(!firstRun.empty(), "data/dev-first-run.ini is missing");
        {
            const fs::path file = scratch.Clean("first-run") / kFileName;
            Check(thief_oracle_view::RunOracle(file.string()).loaded, "the oracle did not load its own first run");
            Check(ReadBytes(file) == firstRun, "the oracle's first-run output differs from data/dev-first-run.ini");
        }

        // Fresh equals upgrade: over Defaults.ini at the built-in values, the dev build's
        // first-run output imports into a CameraUnlock.ini that is the committed file, which is
        // what a fresh install creates.
        {
            const fs::path dir = scratch.Clean("fresh-equals-upgrade");
            WriteBytes(dir / kFileName, firstRun);
            Check(cfg::ConfigOwner<Config>(OwnerOptions(dir, g_builtinDefaults)).Load().status == ConfigLoadStatus::Migrated &&
                      ReadBytes(dir / kConfigFileName) == tally.committed,
                  "the dev first-run output does not import into the committed file");
        }

        const std::vector<Input> inputs = Inputs(firstRun);
        std::printf("%zu inputs\n", inputs.size());
        std::printf("comparison 1, the oracle (dev adfccf8) against the import:\n");
        for (const char* d : kComparison1Differences) std::printf("  recorded difference: %s\n", d);
        for (const Input& input : inputs) {
            const ImportRun import = Comparison1(scratch, input);
            std::optional<ImportResult> mapped;
            if (input.bytes && import.result.status != legacy::ReadStatus::Refused) {
                mapped = RunMappedImport(scratch, input);
                Check(mapped->status == ImportStatus::Imported, input.name + ": the mapped import is not Imported");
            }
            for (const fs::path& defaults : {g_builtinDefaults, g_alteredDefaults}) {
                Comparison2(scratch, input, import, mapped ? &*mapped : nullptr, defaults, tally);
            }
        }

        std::printf("comparison 2, the import against the migration, %zu distinct files:\n", tally.migrated.size());
        for (const auto& [over, run] : {std::pair<const char*, const Tally::Run*>{"at the built-in values", &tally.builtin},
                                        std::pair<const char*, const Tally::Run*>{"changed", &tally.altered}}) {
            std::printf("  over Defaults.ini %s: %d created, %d imported (%d holding a default row, %d a value), "
                        "%d deferred, %d refused as dev refused them\n",
                        over, run->created, run->imported, run->with_default_rows, run->with_values, run->deferred,
                        run->refused);
            Check(run->deferred > 0, std::string("no input is deferred over ") + over);
            Check(run->refused > 0, std::string("no input is refused over ") + over);
            Check(run->with_default_rows > 0, std::string("no import writes default over ") + over);
            Check(run->with_values > 0, std::string("no import writes a value over ") + over);
        }
        std::printf("  %d with a changed sensitivity, inversion or scale dropped (pose_shaping)\n",
                    tally.with_pose_shaping_dropped);
        std::printf("  %d with MoveCrosshair=false dropped (reticle)\n", tally.with_reticle_dropped);
        std::printf("  %d with [Collision] Enabled=false following the default (follows_default)\n",
                    tally.with_follows_default);
        std::printf("  deferred: %s\n", kUnrepresentable);
        Check(tally.with_pose_shaping_dropped > 0, "no input drops a changed pose-shaping value");
        Check(tally.with_reticle_dropped > 0, "no input drops MoveCrosshair");
        Check(tally.with_follows_default > 0, "no input has [Collision] Enabled follow the default");
        Check(tally.migrated.count(tally.committed) == 1, "no input migrated to the committed file");

        wchar_t exe[MAX_PATH];
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        const fs::path lintDir = fs::path(exe).parent_path() / "migrated";
        fs::remove_all(lintDir);
        fs::create_directories(lintDir);
        int n = 0;
        for (const std::string& file : tally.migrated) WriteBytes(lintDir / (std::to_string(n++) + ".ini"), file);
    } catch (const std::exception& e) {
        std::printf("  FAIL: threw: %s\n", e.what());
        ++g_failures;
    }

    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
