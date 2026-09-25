// The config differential test (convert-a-mod-to-the-canonical-config, section 5).
//
// Oracle: the reader of the newest published build, the rolling `dev` pre-release at
// adfccf8, with the core sources it compiled at its pin f4135c4 (oracle_adapter.h).
// Import: the frozen reader in src/legacy_config/.
//
// Comparison 1, oracle against import, on every input: load status, every field both read
// (floats bit for bit), the startup state, and which actions every key press fires under
// every set of held modifiers. The differences it may find are kComparison1Differences
// below.
//
// The key presses go through the published build's own Hotkeys::Start and the guards it
// compiled (OracleFires).

#include "legacy_config/legacy_config.h"
#include "oracle_adapter.h"

#include "cameraunlock/config/testing/ini_mutations.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
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

class Scratch {
public:
    Scratch() {
        root_ = fs::temp_directory_path() / ("thief-config-differential-" + std::to_string(GetCurrentProcessId()));
        fs::remove_all(root_);
        fs::create_directories(root_);
    }
    ~Scratch() {
        std::error_code ec;
        for (const auto& e : fs::recursive_directory_iterator(root_, ec)) {
            if (e.is_regular_file()) SetFileAttributesW(e.path().c_str(), FILE_ATTRIBUTE_NORMAL);
        }
        fs::remove_all(root_, ec);
    }
    fs::path Fresh(const std::string& leaf) {
        const fs::path dir = root_ / (leaf + std::to_string(next_++));
        fs::create_directories(dir);
        return dir;
    }

private:
    fs::path root_;
    int next_ = 0;
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
    const fs::path dir = scratch.Fresh("import");
    const fs::path file = Place(dir, input);
    if (input.bytes) SetReadOnly(file, true);
    const Listing before = List(dir);
    ImportRun run;
    run.result = run.config.Read(file.string().c_str());
    Check(List(dir) == before, input.name + ": the import changed its folder");
    return run;
}

ImportRun Comparison1(Scratch& scratch, const Input& input) {
    const fs::path odir = scratch.Fresh("oracle");
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

        // The dev build's first-run output, committed once as test data, is what the oracle
        // still writes for a missing file.
        const std::string firstRun = ReadBytes(fs::path(THIEF_DIFFERENTIAL_DATA) / "dev-first-run.ini");
        Check(!firstRun.empty(), "data/dev-first-run.ini is missing");
        {
            const fs::path dir = scratch.Fresh("first-run");
            const fs::path file = dir / kFileName;
            Check(thief_oracle_view::RunOracle(file.string()).loaded, "the oracle did not load its own first run");
            Check(ReadBytes(file) == firstRun, "the oracle's first-run output differs from data/dev-first-run.ini");
        }

        const std::vector<Input> inputs = Inputs(firstRun);
        std::printf("comparison 1 (oracle dev adfccf8 against the import) on %zu inputs\n", inputs.size());
        for (const char* d : kComparison1Differences) std::printf("  recorded difference: %s\n", d);
        for (const Input& input : inputs) Comparison1(scratch, input);
    } catch (const std::exception& e) {
        std::printf("  FAIL: threw: %s\n", e.what());
        ++g_failures;
    }

    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
