// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo
//
// The pure logic the hooks inject through, exercised without a game.
//
// Six things are covered here, and each one is a place a wrong answer is invisible in
// play: the INI sanitizers and the CALL SITES that use them, the guard every engine
// memory read passes through, the UE3 rotator maths at the engine boundary, the reticle
// projection, the zoom factor, and the lean trace's own geometry. Everything here is a
// behaviour lock - the assertions record what the shipped code does, so a restructure
// that changes an answer fails rather than ships.

#include "aim_projection.h"
#include "config.h"
#include "config_sanitize.h"
#include "build_profile.h"
#include "hud_basis.h"
#include "lean_geometry.h"
#include "memory_probe.h"
#include "ue3_math.h"
#include "zoom_factor.h"

#include <windows.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

int g_failures = 0;

void Check(bool ok, const char* what) {
    if (!ok) {
        ++g_failures;
        std::printf("FAIL: %s\n", what);
    }
}

// Floats come back through printf/strtod round trips and through trig, so an exact
// comparison would be testing the decimal formatter rather than the behaviour.
bool Near(float a, float b, float tolerance) {
    return std::fabs(a - b) <= tolerance;
}

void CheckNear(float actual, float expected, const char* what, float tolerance = 1e-4f) {
    if (!Near(actual, expected, tolerance)) {
        ++g_failures;
        std::printf("FAIL: %s (expected %g, got %g)\n", what, expected, actual);
    }
}

using namespace ThiefHeadTracking;

const float kNan = std::nanf("");
const float kInf = HUGE_VALF;

// ---------------------------------------------------------------------------
// config_sanitize.h - the boundary validation every INI float passes through.
// ---------------------------------------------------------------------------

void SanitizersRejectOnlyWhatTheyMust() {
    CheckNear(SanitizeFinite(2.5f, 1.0f), 2.5f, "a finite value is passed through");
    CheckNear(SanitizeFinite(-3.0f, 1.0f), -3.0f,
              "a negative finite value is legitimate tuning");
    CheckNear(SanitizeFinite(kNan, 1.0f), 1.0f, "NaN lands on the fallback");
    CheckNear(SanitizeFinite(kInf, 1.0f), 1.0f, "infinity lands on the fallback");

    // Validation, never a floor: 0.0 is a value the user can choose.
    CheckNear(SanitizeSmoothing(0.0f, 0.15f), 0.0f,
              "smoothing 0 survives - there is no floor");
    CheckNear(SanitizeSmoothing(0.42f, 0.15f), 0.42f, "a value inside [0,1] is untouched");
    CheckNear(SanitizeSmoothing(1.5f, 0.15f), 1.0f, "smoothing above 1 clamps to 1");
    CheckNear(SanitizeSmoothing(-1.0f, 0.15f), 0.0f, "smoothing below 0 clamps to 0");
    CheckNear(SanitizeSmoothing(kNan, 0.15f), 0.15f,
              "a non-finite smoothing lands on its own default");

    // A negative limit inverts the processor's clamp and pins the camera at a constant
    // offset, which reads in game as positional tracking having died.
    CheckNear(SanitizePositiveLimit(0.9f, 0.4f), 0.9f,
              "a widened limit is tuning, not an error");
    CheckNear(SanitizePositiveLimit(-0.4f, 0.4f), 0.4f,
              "a negative limit lands on the fallback");
    CheckNear(SanitizePositiveLimit(0.0f, 0.4f), 0.4f, "a zero limit lands on the fallback");
    CheckNear(SanitizePositiveLimit(kInf, 0.4f), 0.4f,
              "a non-finite limit lands on the fallback");
}

// ---------------------------------------------------------------------------
// Config::LoadOrCreate - the sanitizers were already covered, the call sites were not.
// ---------------------------------------------------------------------------

std::string TempIniPath(const char* leaf) {
    char dir[MAX_PATH];
    const DWORD written = GetTempPathA(MAX_PATH, dir);
    if (written == 0 || written >= MAX_PATH) {
        return std::string();
    }
    return std::string(dir) + leaf;
}

void WriteIni(const std::string& path, const char* body) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (f == nullptr) {
        ++g_failures;
        std::printf("FAIL: could not write the test INI at %s\n", path.c_str());
        return;
    }
    std::fputs(body, f);
    std::fclose(f);
}

// The text of an INI, or an empty string when it could not be read.
std::string ReadWholeFile(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) return {};
    std::string out;
    char buffer[4096];
    size_t got;
    while ((got = std::fread(buffer, 1, sizeof(buffer), f)) > 0) {
        out.append(buffer, got);
    }
    std::fclose(f);
    return out;
}

void AMissingIniIsCreatedWithTheShippedDefaults() {
    const std::string path = TempIniPath("thief_ht_defaults.ini");
    Check(!path.empty(), "a temp directory is available for the config tests");
    if (path.empty()) return;
    DeleteFileA(path.c_str());

    Config cfg;
    Check(cfg.LoadOrCreate(path.c_str()), "a missing INI is created rather than refused");
    Check(GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES,
          "and the file it wrote is on disk");

    // Against the FILE, not against the constants. Every member below is initialised from
    // the same constant the reader falls back to, so comparing the two proves nothing: an
    // INI writer emptied out entirely would satisfy all of it. The shipped INI is the mod's
    // user-facing documentation, so what has to be checked is that the keys reached the
    // disk.
    const std::string text = ReadWholeFile(path);
    Check(!text.empty(), "the created INI has content rather than being an empty file");
    // Each key is looked for as a LINE that assigns it, not as a substring. A bare
    // substring search passes on prose and on longer siblings - "Yaw" is in the WorldSpaceYaw
    // comment, "Enabled" is in the Collision comment, "Toggle" is inside "ChordToggle" and
    // "LimitZ" is inside "LimitZBack" - so eight of these could be deleted from the writer
    // with the suite still green, which is the hole this test was written to close.
    static const char* const kExpectedKeys[] = {
        "EnableOnStartup", "Port", "WorldSpaceYaw", "MoveCrosshair", "AdsMode",
        "Yaw", "Pitch", "Roll", "InvertYaw", "InvertPitch", "InvertRoll",
        "LocalSmoothing", "RemoteSmoothing",
        "Enabled", "SensitivityX", "SensitivityY", "SensitivityZ",
        "LimitX", "LimitY", "LimitZ", "LimitZBack", "PositionScale",
        "Margin", "ReleaseSmoothing", "Channel",
        "Toggle", "CycleMode", "YawMode",
        "ChordToggle", "ChordCycleMode", "ChordYawMode", "ChordAdsMode",
        "StructProbe",
    };
    for (const char* key : kExpectedKeys) {
        if (text.find("\n" + std::string(key) + "=") == std::string::npos) {
            Check(false, key);
            std::printf("      (no line assigning this key in the INI the mod wrote)\n");
        }
    }
    static const char* const kExpectedSections[] = {
        "[General]", "[Sensitivity]", "[Smoothing]", "[Position]", "[Collision]",
        "[Hotkeys]", "[Diagnostics]",
    };
    for (const char* section : kExpectedSections) {
        if (text.find(section) == std::string::npos) {
            Check(false, section);
            std::printf("      (section missing from the INI the mod wrote)\n");
        }
    }
    Check(text.find("\nPort=4242") != std::string::npos,
          "and the port it wrote is the documented default");

    // The retired positional inversion keys must NOT come back: they land ahead of the
    // asymmetric z clamp, which moves the generous forward budget onto the backward lean.
    // Matched as assignments too, so InvertY is distinguishable from InvertYaw.
    Check(text.find("\nInvertX=") == std::string::npos,
          "and the INI offers no positional axis inversion on x");
    Check(text.find("\nInvertY=") == std::string::npos, "nor on y");
    Check(text.find("\nInvertZ=") == std::string::npos, "nor on z");

    Check(cfg.udp_port == kDefaultPort, "the created INI round-trips the default port");
    Check(cfg.enabled_on_startup == kDefaultEnableOnStartup, "and EnableOnStartup");
    Check(cfg.world_space_yaw == kDefaultWorldSpaceYaw, "and WorldSpaceYaw");
    Check(cfg.move_crosshair == kDefaultMoveCrosshair, "and MoveCrosshair");
    Check(cfg.ads_mode == kDefaultAdsMode, "and the ADS mode");
    Check(cfg.collision_enabled == kDefaultCollision, "and the collision switch");
    Check(cfg.struct_probe == kDefaultStructProbe, "and the struct probe switch");
    CheckNear(cfg.local_smoothing, kDefaultLocalSmoothing, "and LocalSmoothing");
    CheckNear(cfg.remote_smoothing, kDefaultRemoteSmoothing, "and RemoteSmoothing");
    CheckNear(cfg.pos_limit_z, kDefaultPosLimitZ, "and the forward lean limit");
    CheckNear(cfg.pos_limit_z_back, kDefaultPosLimitZBack, "and the backward one");
    CheckNear(cfg.position_scale, kDefaultPositionScale,
              "and the metres-to-centimetres scale", 1e-2f);
    CheckNear(cfg.collision_margin, kDefaultCollisionMargin,
              "and the collision margin", 1e-2f);

    // The generous forward budget has to stay on leaning IN, which is what the asymmetry
    // is for.
    Check(cfg.pos_limit_z > cfg.pos_limit_z_back,
          "and the forward lean keeps the larger budget");

    DeleteFileA(path.c_str());
}

void ABadValueLandsOnTheDefaultRatherThanReachingTheCamera() {
    const std::string path = TempIniPath("thief_ht_bad.ini");
    if (path.empty()) return;
    WriteIni(path,
             "[General]\r\n"
             "Port=4242\r\n"
             "AdsMode=marker\r\n"
             "[Sensitivity]\r\n"
             "Yaw=nan\r\n"
             "Pitch=2.5\r\n"
             "[Smoothing]\r\n"
             "LocalSmoothing=5.0\r\n"
             "RemoteSmoothing=-2.0\r\n"
             "[Position]\r\n"
             "LimitZ=-0.4\r\n"
             "LimitX=0.9\r\n"
             "PositionScale=inf\r\n"
             "[Collision]\r\n"
             "Margin=0\r\n"
             "ReleaseSmoothing=9\r\n"
             "Channel=-1\r\n"
             "[Hotkeys]\r\n"
             "Toggle=0x1234\r\n");

    Config cfg;
    Check(cfg.LoadOrCreate(path.c_str()), "a file full of bad values still loads");
    CheckNear(cfg.sens_yaw, kDefaultSensitivity, "a NaN sensitivity lands on the default");
    CheckNear(cfg.sens_pitch, 2.5f, "while the good value beside it is kept");
    CheckNear(cfg.local_smoothing, 1.0f, "smoothing above the domain clamps to 1");
    CheckNear(cfg.remote_smoothing, 0.0f, "smoothing below it clamps to 0");
    CheckNear(cfg.pos_limit_z, kDefaultPosLimitZ,
              "a negative limit lands on the default, keeping the clamp the right way round");
    CheckNear(cfg.pos_limit_x, 0.9f, "while a widened limit beside it is honoured");
    CheckNear(cfg.position_scale, kDefaultPositionScale,
              "a non-finite scale lands on the default", 1e-2f);
    CheckNear(cfg.collision_margin, kDefaultCollisionMargin,
              "a zero margin lands on the default", 1e-2f);
    CheckNear(cfg.collision_release_smoothing, 1.0f,
              "the release pacing clamps into [0,1]");
    // Zero means "use the flags this build pinned"; a negative value is a typo that took
    // the same branch, so it has to land on the pin rather than on a nonsense flag word.
    Check(cfg.collision_channel == kDefaultCollisionChannel,
          "a negative trace channel falls back to the pinned flags");
    Check(cfg.vk_toggle == kDefaultVkToggle,
          "an unusable virtual-key code lands on the documented default");
    // Thief is a two-slot mod, so a three-slot sibling's config cannot select a mode this
    // build does not have.
    Check(cfg.ads_mode == kDefaultAdsMode, "marker is not a mode this mod has");

    DeleteFileA(path.c_str());
}

void ThePortIsTheOneErrorThatRefusesToStart() {
    const std::string path = TempIniPath("thief_ht_port.ini");
    if (path.empty()) return;

    WriteIni(path, "[General]\r\nPort=80\r\n");
    Config below;
    Check(!below.LoadOrCreate(path.c_str()),
          "a port below the bindable range refuses to load");
    DeleteFileA(path.c_str());

    WriteIni(path, "[General]\r\nPort=99999\r\n");
    Config above;
    Check(!above.LoadOrCreate(path.c_str()), "and so does one above it");
    DeleteFileA(path.c_str());

    Config noPath;
    Check(!noPath.LoadOrCreate(""), "an unresolvable directory refuses to load");
    Check(!noPath.LoadOrCreate(nullptr), "and so does a null path");
}

void TheAdsModeSurvivesARestart() {
    const std::string path = TempIniPath("thief_ht_ads.ini");
    if (path.empty()) return;
    DeleteFileA(path.c_str());

    Config written;
    Check(written.LoadOrCreate(path.c_str()), "the ADS test starts from a created INI");
    Check(SaveAdsMode(path.c_str(), AdsMode::Tracked), "the cycled mode is written back");

    Config reloaded;
    Check(reloaded.LoadOrCreate(path.c_str()), "and the file still loads afterwards");
    Check(reloaded.ads_mode == AdsMode::Tracked, "and the choice survived the restart");

    Check(!SaveAdsMode("", AdsMode::Paused),
          "an unresolvable path reports the write failure");
    DeleteFileA(path.c_str());
}

// ---------------------------------------------------------------------------
// memory_probe.h - the guard every engine pointer walk goes through.
// ---------------------------------------------------------------------------

void AnUnreadablePageIsRejectedEvenWhenItIsCommitted() {
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    const SIZE_T pageSize = si.dwPageSize;

    // Three committed pages in one reservation, so each protection below is a region of
    // its own and VirtualQuery reports it separately.
    auto* base = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, pageSize * 3, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Check(base != nullptr, "the probe test could reserve three pages");
    if (base == nullptr) return;

    const auto addr = [base, pageSize](int page) {
        return reinterpret_cast<std::uintptr_t>(base + pageSize * page);
    };

    DWORD old = 0;
    Check(VirtualProtect(base + pageSize, pageSize, PAGE_NOACCESS, &old) != FALSE,
          "and mark its second page PAGE_NOACCESS");
    Check(VirtualProtect(base + pageSize * 2, pageSize, PAGE_READWRITE | PAGE_GUARD, &old)
              != FALSE,
          "and its third a guard page");

    Check(ReadableSpan(addr(0), 8), "a committed readable page reads");
    // Both of these are MEM_COMMIT. Answering yes to either is a fault on the next
    // dereference, inside a detour on the render path.
    Check(!ReadableSpan(addr(1), 8), "a committed PAGE_NOACCESS page does not");
    Check(!ReadableSpan(addr(2), 8), "and neither does a committed guard page");

    // The span is the question, not the first byte of it: a read that starts on a good
    // page and runs off the end of the region is still a fault.
    Check(ReadableSpan(addr(0), pageSize), "a span filling the region is readable");
    Check(!ReadableSpan(addr(0), pageSize + 1),
          "a span running past the region's end is not");

    Check(!ReadableSpan(0, 8), "and a null address is refused outright");

    // Readable adds the object-pointer test; ReadableSpan deliberately does not, because
    // a struct field - a DWORD of bitflags, say - is routinely 4-aligned and rejecting
    // its address would silently switch off whatever gate read it.
    Check(!Readable(addr(0) + 4, 4), "an unaligned address is not an object pointer");
    Check(ReadableSpan(addr(0) + 4, 4), "but it is a perfectly good field address");

    VirtualFree(base, 0, MEM_RELEASE);
}

// ---------------------------------------------------------------------------
// ue3_math.h - the rotation boundary.
// ---------------------------------------------------------------------------

void RotatorUnitsConvertTheWayTheEngineDoes() {
    Check(NormalizeUnits(60000) == -5536,
          "the positive-wrapped form normalises to the signed one");
    Check(NormalizeUnits(-5536) == -5536, "a signed value is already normal");
    Check(NormalizeUnits(16384) == 16384, "and a quarter turn is unchanged");

    Check(DegToUnits(90.0f) == 16384, "90 degrees is a quarter turn of rotator units");
    Check(DegToUnits(-90.0f) == -16384, "and so is the other way");
    Check(DegToUnits(0.0f) == 0, "zero converts to zero");
    Check(DegToUnits(360.0f) == 65536, "a full turn still converts to a full turn");
    // Reduced by whole turns only past the point the narrowing conversion would be
    // undefined, so an enormous INI sensitivity cannot overflow int32.
    Check(DegToUnits(1e9f) == DegToUnits(std::fmod(1e9f, 360.0f)),
          "an enormous angle is reduced modulo a turn rather than overflowing");

    Check(ClampPitchUnits(20000) == kMaxPitchUnits, "pitch is held inside a quarter turn");
    Check(ClampPitchUnits(-20000) == -kMaxPitchUnits, "in both directions");
    Check(ClampPitchUnits(1000) == 1000, "and left alone inside it");
    Check(kMaxPitchUnits == 16384, "the quarter turn is a quarter of the rotator range");
}

void ThePitchBoundLimitsTheHeadsContributionNotTheSum() {
    Check(BoundedPitchContribution(0, 45.0f) == DegToUnits(45.0f),
          "a head pitch with room to move is passed through whole");
    Check(BoundedPitchContribution(16000, 45.0f) == kMaxPitchUnits - 16000,
          "near the limit only the remaining room is granted");
    Check(BoundedPitchContribution(kMaxPitchUnits, 45.0f) == 0,
          "at the limit nothing more is granted");
    Check(BoundedPitchContribution(-16000, -45.0f) == -kMaxPitchUnits + 16000,
          "and the same holds looking down");
    // The positive-wrapped form has to be normalised first, or every downward look reads
    // as far past the limit and the view pins at the sky.
    Check(BoundedPitchContribution(60000, 45.0f) == DegToUnits(45.0f),
          "a positive-wrapped clean pitch is normalised before it is bounded");
}

void ComposingAZeroHeadPoseLeavesTheViewpointAlone() {
    const UE3Rotator clean = { 3000, -12000, 500 };

    UE3Rotator world = clean;
    ComposeHeadRotation(clean, 0.0f, 0.0f, 0.0f, /*worldSpaceYaw=*/true, &world);
    Check(world.Pitch == clean.Pitch && world.Yaw == clean.Yaw && world.Roll == clean.Roll,
          "world-space yaw with no head pose is the identity");

    UE3Rotator local = clean;
    ComposeHeadRotation(clean, 0.0f, 0.0f, 0.0f, /*worldSpaceYaw=*/false, &local);
    // The matrix path round-trips through trig, so it lands within a rotator unit or two
    // rather than exactly.
    Check(std::abs(local.Pitch - NormalizeUnits(clean.Pitch)) <= 2 &&
              std::abs(local.Yaw - NormalizeUnits(clean.Yaw)) <= 2 &&
              std::abs(local.Roll - NormalizeUnits(clean.Roll)) <= 2,
          "camera-local yaw with no head pose round-trips back to the viewpoint");
}

void WorldSpaceYawIsPlainRotatorAddition() {
    const UE3Rotator clean = { 0, 8192, 0 };
    UE3Rotator rot = clean;
    ComposeHeadRotation(clean, 0.0f, 10.0f, 5.0f, /*worldSpaceYaw=*/true, &rot);
    Check(rot.Yaw == 8192 + DegToUnits(10.0f), "head yaw adds to the viewpoint's yaw");
    Check(rot.Roll == -DegToUnits(5.0f),
          "and head roll to its roll, negated at the engine boundary");
    Check(rot.Pitch == 0, "with pitch left at the bounded sum");
}

void TheAimResolvesToStraightAheadWhenTheHeadIsCentred() {
    const UE3Rotator clean = { 2000, -9000, 0 };
    float ruf[3];
    ResolveAimInTrackedView(clean, clean, ruf);
    CheckNear(ruf[0], 0.0f, "an untracked view puts the aim dead ahead: no right offset");
    CheckNear(ruf[1], 0.0f, "no up offset");
    CheckNear(ruf[2], 1.0f, "and a unit forward component");
}

void CleanRollNeverReachesTheAimDirection() {
    const UE3Rotator upright = { 2000, -9000, 0 };
    const UE3Rotator rolled  = { 2000, -9000, 9000 };
    const UE3Rotator tracked = { 1000, -8000, 0 };
    float a[3], b[3];
    ResolveAimInTrackedView(upright, tracked, a);
    ResolveAimInTrackedView(rolled, tracked, b);
    // Row 0 of the rotation matrix carries no roll term, so the clean roll cannot move
    // where the shot is going.
    CheckNear(a[0], b[0], "clean roll leaves the aim's right component alone");
    CheckNear(a[1], b[1], "and its up component");
    CheckNear(a[2], b[2], "and its forward component");
}

void APointResolvesAgainstTheRenderEye() {
    const UE3Vector eye = { 0.0f, 0.0f, 0.0f };
    const UE3Vector ahead = { 100.0f, 0.0f, 0.0f };
    const UE3Rotator identity = { 0, 0, 0 };
    float ruf[3];
    ResolvePointInTrackedView(eye, ahead, identity, ruf);
    CheckNear(ruf[0], 0.0f, "a point on the forward axis has no right offset");
    CheckNear(ruf[1], 0.0f, "and no up offset");
    CheckNear(ruf[2], 100.0f, "and its forward component is the distance to it");

    // The lean is what makes this a point rather than a direction: move the render eye
    // sideways and the same world point moves across the view.
    const UE3Vector leaned = { 0.0f, 30.0f, 0.0f };
    ResolvePointInTrackedView(leaned, ahead, identity, ruf);
    CheckNear(ruf[0], -30.0f, "leaning right moves the impact point left in the view");
    CheckNear(ruf[2], 100.0f, "at the same depth");
}

// ---------------------------------------------------------------------------
// zoom_factor.h - exactly 1.0 in ordinary play, and never above it.
// ---------------------------------------------------------------------------

// Values a player can plausibly type that the reader must not act on.
//
// Two of these BITE if their fix is reverted: a negative position sensitivity comes back as
// -1.0, and a trace channel above 0x7FFFFFFF comes back saturated to 0x7FFFFFFF. The rest
// are behaviour locks rather than regression tests, and deliberately so - the fix for an
// unparseable float or hotkey is the WARN it now writes, and the value it settles on is the
// same default the old reader silently chose, so no assertion on the config can tell the
// two apart. Proving those would need the log itself under test, which nothing here can
// see; they are kept because the value they lock is still the one the player must get.
void AnUnusableValueIsRefusedRatherThanQuietlyReplaced() {
    const std::string path = TempIniPath("thief_ht_refused.ini");
    if (path.empty()) return;
    WriteIni(path,
             "[General]\n"
             "EnableOnStartup=0 ; I like it off\n"
             "[Position]\n"
             "SensitivityX=-1.0\n"
             "SensitivityY=2.0\n"
             "[Collision]\n"
             "Margin=twenty\n"
             "Channel=0xFFFFFFFF\n"
             "[Hotkeys]\n"
             "Toggle=zzz\n");

    Config cfg;
    Check(cfg.LoadOrCreate(path.c_str()), "an INI full of bad values still loads");

    // A trailing "; comment" is part of the value, so this bool is unusable rather than
    // false. Taking it as false would honour an edit the reader cannot actually parse.
    Check(cfg.enabled_on_startup == kDefaultEnableOnStartup,
          "a bool with a trailing comment falls back rather than being guessed at");

    // A negative position sensitivity is an axis inversion in disguise, and it lands ahead
    // of the asymmetric z clamp.
    CheckNear(cfg.pos_sens_x, kDefaultPosSens,
              "a negative position sensitivity is refused");
    CheckNear(cfg.pos_sens_y, 2.0f, "while a legitimate one beside it is honoured");

    CheckNear(cfg.collision_margin, kDefaultCollisionMargin,
              "a collision margin that is not a number is refused", 1e-2f);

    // The whole 32-bit word, not the half strtol can carry: saturating this to 0x7FFFFFFF
    // would silently drop bit 31 from a flag word the user asked for.
    Check(cfg.collision_channel == 0xFFFFFFFFu,
          "a trace channel above 0x7FFFFFFF survives intact");

    Check(cfg.vk_toggle == kDefaultVkToggle,
          "a hotkey that is not a hex number falls back to the documented default");

    DeleteFileA(path.c_str());
}

void TheZoomFactorIsOneUnlessTheGameIsZoomingIn() {
    CheckNear(ZoomFactor(90.0f, 90.0f), 1.0f, "an un-zoomed frame scales the head by 1");
    CheckNear(ZoomFactor(120.0f, 90.0f), 1.0f,
              "a WIDER field of view is passed through rather than amplified");

    const float kPi = 3.14159265358979f;
    CheckNear(ZoomFactor(45.0f, 90.0f),
              std::tan(45.0f * 0.5f * kPi / 180.0f) / std::tan(90.0f * 0.5f * kPi / 180.0f),
              "a zoom scales by the ratio of the half-angle tangents", 1e-3f);
    Check(ZoomFactor(45.0f, 90.0f) < 1.0f, "and that ratio shrinks the head pose");

    // An unreadable field of view means no compensation, never a guessed one.
    CheckNear(ZoomFactor(0.0f, 90.0f), 1.0f, "an out-of-range live FOV applies no scaling");
    CheckNear(ZoomFactor(90.0f, 0.0f), 1.0f, "nor does an out-of-range base");
    CheckNear(ZoomFactor(kNan, 90.0f), 1.0f, "nor a non-finite one");
}

// ---------------------------------------------------------------------------
// lean_geometry.h - the trace's own arithmetic, which the core policy is unaware of.
// ---------------------------------------------------------------------------

void TheLeanTraceReachesPastTheLean() {
    // A ray that stopped where the lean stops cannot see the surface the lean is about to
    // come to rest against.
    CheckNear(LeanTraceLength(30.0f, 20.0f), 30.0f - 20.0f + 20.0f / kMinApproachCos,
              "the trace overreaches the lean by the margin at the shallowest approach");
    Check(LeanTraceLength(30.0f, 20.0f) > 30.0f, "so it is longer than the lean itself");
}

void AGlancingHitStopsTheLeanFurtherBack() {
    CheckNear(LeanContactDistance(50.0f, 1.0f, 20.0f), 50.0f,
              "a head-on hit needs no extra pull-back");
    CheckNear(LeanContactDistance(50.0f, 0.5f, 20.0f), 30.0f,
              "a 60 degree approach costs a margin's worth again");
    // Floored rather than diverging: 1/cos runs away as the lean turns parallel to the
    // surface, so the pull-back is computed at kMinApproachCos and no shallower.
    CheckNear(LeanContactDistance(200.0f, 0.05f, 20.0f),
              200.0f - (20.0f / kMinApproachCos - 20.0f),
              "a glancing approach takes the floored pull-back rather than diverging");
    CheckNear(LeanContactDistance(200.0f, -1.0f, 20.0f),
              200.0f - (20.0f / kMinApproachCos - 20.0f),
              "a degenerate normal reads as glancing, which fails toward stopping short");
    // Never negative: a pull-back deeper than the hit means the eye may not move at all.
    CheckNear(LeanContactDistance(5.0f, 0.05f, 20.0f), 0.0f,
              "and a pull-back deeper than the hit stops the lean outright");
    CheckNear(LeanContactDistance(50.0f, 0.05f, 20.0f), 0.0f,
              "including when the hit is well short of the floored pull-back");

    // Bounded ABOVE as well. A cosine cannot exceed 1, but the normal it comes from is read
    // at an inferred struct offset and is not guaranteed to be a unit vector. Unbounded, the
    // pull-back goes negative and this returns a contact FURTHER away than the trace found
    // it - the policy's skin then cancels against it and the eye is let onto the surface,
    // inside the near plane, which is the see-through-the-wall failure the stand-off exists
    // to prevent.
    CheckNear(LeanContactDistance(100.0f, 5.0f, 20.0f), 100.0f,
              "an over-unit cosine is capped rather than pushing the contact away");
    Check(LeanContactDistance(100.0f, 5.0f, 20.0f) <= 100.0f,
          "so the contact is never reported further off than the trace measured");
    Check(LeanContactDistance(100.0f, 1e30f, 20.0f) <= 100.0f, "at any magnitude");
}

// The six litmus tests AGENTS.md names as the gate on reticle compensation, run against
// the composition the camera actually applies rather than against a re-derived formula.
// ComposeHeadRotation builds the rotator the engine is handed, and ResolveAimInTrackedView
// resolves the clean aim in the basis that rotator renders, so these two together are the
// whole path a drifting reticle would drift along.

// ---------------------------------------------------------------------------
// hud_basis.h - the world-to-clip matrix the game's own HUD projects through.
// ---------------------------------------------------------------------------

// Builds a matrix in the shape the game caches: columns are the view axes, column 1 scaled
// by the projection's vertical term, column 3 a second copy of the forward, and the bottom
// row the translation -dot(eye, axis) * that column's length.
void BuildHudMatrix(const UE3Rotator& rot, const float eye[3], float upScale, float wScale,
                    float m[16]) {
    const Mat3 t = RotatorToMatrix(rot);
    const float* axis[4] = { t.m[1], t.m[2], t.m[0], t.m[0] };
    const float len[4] = { 1.0f, upScale, 1.0f, wScale };
    for (int j = 0; j < 4; ++j) {
        m[j]      = axis[j][0] * len[j];
        m[4 + j]  = axis[j][1] * len[j];
        m[8 + j]  = axis[j][2] * len[j];
        m[12 + j] = -(eye[0] * axis[j][0] + eye[1] * axis[j][1] + eye[2] * axis[j][2]) *
                    len[j];
    }
}

// A world point through the matrix, the way the game does it: v * M, then the perspective
// divide by the w column.
void ProjectThroughHudMatrix(const float m[16], const float p[3], float outXy[2]) {
    float clip[4];
    for (int j = 0; j < 4; ++j) {
        clip[j] = p[0] * m[j] + p[1] * m[4 + j] + p[2] * m[8 + j] + m[12 + j];
    }
    outXy[0] = clip[0] / clip[3];
    outXy[1] = clip[1] / clip[3];
}

// ---------------------------------------------------------------------------
// build_profile.cpp - the shipped profile's own numbers.
// ---------------------------------------------------------------------------

// The profile is a positional brace-init list, so declaring a new field in one place and
// initialising it in another silently shifts every value below it. That has happened: two
// fields added between vtGetViewTarget and offControllerPawn in the header, but after
// offControllerPawn in the initialiser, handed the Pawn offset a vtable slot. The gate then
// reported "no pawn" on every frame of the session and head tracking never ran, with no
// error anywhere - the mod simply did nothing. Pin the values that matter here so the next
// insertion fails the suite instead.
void TheShippedProfileHasItsOffsetsInTheRightFields() {
    Check(kKnownProfileCount >= 1, "there is at least one build profile");
    const BuildProfile& p = kKnownProfiles[0];
    Check(p.rvaGetPlayerViewPoint == 0x39A2C0u, "the viewpoint accessor RVA");
    Check(p.rvaCalcSceneView == 0x3F6500u, "CalcSceneView's RVA");
    Check(p.rvaCalcSceneViewReturn == 0x3F66C3u, "the scene-view caller's return address");
    Check(p.offPlayerCamera == 0x450u, "AController::PlayerCamera");
    Check(p.offCamDefaultFov == 0x2A4u, "ACamera::DefaultFOV");
    Check(p.offWorldInfoPauser == 0x8F0u, "AWorldInfo::Pauser");
    Check(p.vtGetViewTarget == 0x8F8u, "the GetViewTarget vtable slot");
    Check(p.offControllerPawn == 0x294u, "AController::Pawn");
    Check(p.offCamHudBasis == 0xAF0u, "the camera's HUD projection matrix");
    Check(p.rvaCameraHudMatrix == 0xBE7F00u, "the camera HUD matrix accessor");
    Check(p.rvaCameraHudProject == 0xBE7F30u, "the camera HUD projection");
    Check(p.rvaCameraForward == 0xBEB290u, "the camera forward accessor");
    Check(p.rvaHudMatrixCaller == 0xBC7F58u, "the mission matrix caller");
    Check(p.rvaHudForwardCaller == 0xBC8209u, "the mission forward caller");
    Check(p.rvaWorldTrace == 0x396FE0u, "the script trace wrapper");
    Check(p.rvaGfxSetPosition == 0x794EA0u, "the native movie clip positioning function");
    Check(p.rvaGfxGetVisibleFrameRect == 0x77BD70u, "the movie viewport bounds");
    Check(p.offSceneProjection == 0x110u, "the rendered projection matrix");
}

void RewritingTheMatrixWithItsOwnPoseChangesNothing() {
    const UE3Rotator clean = { 3000, -12000, 0 };
    const float eye[3] = { 120.0f, -3400.0f, 88.0f };
    float original[16], m[16];
    BuildHudMatrix(clean, eye, 2.48f, 1.02f, original);
    for (int i = 0; i < 16; ++i) m[i] = original[i];

    const float noLean[3] = { 0.0f, 0.0f, 0.0f };
    RewriteHudMatrix(m, clean, noLean);
    // The identity is what makes this safe to run on every tick: a frame whose pose is the
    // one the game already chose must come out byte-for-byte where it went in, or the write
    // is disturbing something of its own.
    for (int i = 0; i < 16; ++i) {
        CheckNear(m[i], original[i], "rewriting with the matrix's own pose is an identity",
                  1e-2f);
    }
}

void TheRewrittenMatrixProjectsFromTheTrackedView() {
    const UE3Rotator clean   = { 0, 0, 0 };
    const UE3Rotator tracked = { 0, DegToUnits(22.0f), 0 };
    const float eye[3] = { -500.0f, 250.0f, 60.0f };
    // A point off to one side, so a yaw moves it.
    const float point[3] = { eye[0] + 800.0f, eye[1] + 300.0f, eye[2] + 40.0f };

    float rewritten[16], reference[16];
    BuildHudMatrix(clean, eye, 2.48f, 1.02f, rewritten);
    BuildHudMatrix(tracked, eye, 2.48f, 1.02f, reference);

    const float noLean[3] = { 0.0f, 0.0f, 0.0f };
    RewriteHudMatrix(rewritten, tracked, noLean);

    float a[2], b[2];
    ProjectThroughHudMatrix(rewritten, point, a);
    ProjectThroughHudMatrix(reference, point, b);
    // Against a matrix BUILT for the tracked view, not against a re-derived formula: the
    // whole failure this guards is a rewrite that moves the axes and leaves the bottom row
    // measuring the old ones, which projects every anchor against a matrix describing no
    // camera at all.
    CheckNear(a[0], b[0], "the rewritten matrix projects where a tracked one would", 1e-3f);
    CheckNear(a[1], b[1], "in y as well as x", 1e-3f);
}

void TheRewrittenMatrixKeepsTheProjectionTerms() {
    const UE3Rotator clean   = { 1000, 4000, 0 };
    const UE3Rotator tracked = { 1000, 4000 + DegToUnits(15.0f), 0 };
    const float eye[3] = { 10.0f, 20.0f, 30.0f };
    float m[16];
    BuildHudMatrix(clean, eye, 2.48f, 1.02f, m);

    const float noLean[3] = { 0.0f, 0.0f, 0.0f };
    RewriteHudMatrix(m, tracked, noLean);

    // Column 1 carries the vertical term and column 3 whatever separates the w column from
    // the depth column. Neither has anything to do with where the head is pointing, so both
    // lengths have to survive - rebuilding them as unit vectors would flatten the vertical
    // field of view and break the divide.
    const float up = std::sqrt(m[1] * m[1] + m[5] * m[5] + m[9] * m[9]);
    const float w  = std::sqrt(m[3] * m[3] + m[7] * m[7] + m[11] * m[11]);
    CheckNear(up, 2.48f, "the up column keeps the vertical projection term", 1e-3f);
    CheckNear(w, 1.02f, "and the w column keeps its own scale", 1e-3f);
}

void ALeanMovesTheAnchorsWithTheEye() {
    const UE3Rotator pose = { 0, 0, 0 };
    const float eye[3] = { 0.0f, 0.0f, 0.0f };
    const float point[3] = { 400.0f, 0.0f, 0.0f };
    float m[16];
    BuildHudMatrix(pose, eye, 2.48f, 1.0f, m);

    float before[2];
    ProjectThroughHudMatrix(m, point, before);

    // Lean 30 cm to the camera's right (world +Y with this rotator) and the point ahead has
    // to slide left, exactly as it does in the rendered frame.
    const float lean[3] = { 0.0f, 30.0f, 0.0f };
    RewriteHudMatrix(m, pose, lean);
    float after[2];
    ProjectThroughHudMatrix(m, point, after);
    Check(after[0] < before[0] - 1e-3f, "a lean right moves a point ahead to the left");
}

void ADegenerateMatrixIsLeftAlone() {
    float m[16] = {};
    float copy[16] = {};
    const float noLean[3] = { 0.0f, 0.0f, 0.0f };
    // What an unbuilt matrix looks like on the first frames of a level. There is no eye to
    // recover from it, so there is nothing to rotate - and a rewrite that pressed on would
    // hand the HUD a matrix built around an eye of zero.
    RewriteHudMatrix(m, { 1000, 2000, 0 }, noLean);
    for (int i = 0; i < 16; ++i) {
        Check(m[i] == copy[i], "an unbuilt matrix is left exactly as it was");
    }
}

void HeadRollAloneLeavesTheAimAtScreenCentre() {
    const UE3Rotator clean = { 0, 0, 0 };
    UE3Rotator tracked = clean;
    ComposeHeadRotation(clean, 0.0f, 0.0f, 25.0f, /*worldSpaceYaw=*/false, &tracked);
    float ruf[3];
    ResolveAimInTrackedView(clean, tracked, ruf);
    CheckNear(ruf[0], 0.0f, "pure head roll leaves the aim's right offset at zero");
    CheckNear(ruf[1], 0.0f, "and its up offset");
}

void HeadPitchAloneMovesTheAimVerticallyOnly() {
    const UE3Rotator clean = { 0, 0, 0 };
    UE3Rotator tracked = clean;
    ComposeHeadRotation(clean, 15.0f, 0.0f, 0.0f, /*worldSpaceYaw=*/false, &tracked);
    float ruf[3];
    ResolveAimInTrackedView(clean, tracked, ruf);
    CheckNear(ruf[0], 0.0f, "pure head pitch leaves the aim's right offset at zero");
    Check(std::fabs(ruf[1]) > 0.1f, "and moves it vertically");
}

void HeadPitchAndRollRotateTheOffsetWithoutResizingIt() {
    // With the head pitched, roll turns the aim offset about screen centre. The offset
    // resolves to sin(pitch) * (sin roll, -cos roll), so its LENGTH is sin(pitch) whatever
    // the roll - which makes a magnitude test vacuous: it holds for any orthonormal basis,
    // for any roll magnitude, and for a roll of the wrong SIGN. The assertion that bites is
    // on the angle, because that is what a sign flip moves, and a roll sign that disagrees
    // between the camera and the reticle is the fault AGENTS.md records as having shipped.
    const UE3Rotator clean = { 0, 0, 0 };
    const float expectedLength = std::sin(15.0f * 3.14159265358979f / 180.0f);
    for (float roll = 0.0f; roll <= 60.0f; roll += 15.0f) {
        UE3Rotator tracked = clean;
        ComposeHeadRotation(clean, 15.0f, 0.0f, roll, /*worldSpaceYaw=*/false, &tracked);
        float ruf[3];
        ResolveAimInTrackedView(clean, tracked, ruf);

        const float degrees =
            std::atan2(ruf[0], -ruf[1]) * 180.0f / 3.14159265358979f;
        // Against the NEGATED roll, because that is the roll ComposeHeadRotation wrote
        // into the rotator the frame is drawn with. Asserting the tracker's own sign here
        // would pass only while the boundary conversion was missing, which is the state
        // this game shipped with and the one the player reported.
        CheckNear(degrees, -roll,
                  "head roll turns the pitched aim offset by exactly the roll the camera "
                  "was given",
                  1e-2f);
        CheckNear(std::sqrt(ruf[0] * ruf[0] + ruf[1] * ruf[1]), expectedLength,
                  "and leaves its length at sin(pitch)", 1e-3f);
    }
}

void WorldSpaceYawLookingStraightDownLeavesTheAimAtCentre() {
    // Looking straight down, a yaw about world up is a pure spin about the view axis: the
    // world turns and the shot still goes exactly where it was going. A projection that
    // treated head yaw as camera-local would sweep the reticle through an arc here.
    const UE3Rotator clean = { DegToUnits(-90.0f), 4096, 0 };
    UE3Rotator tracked = clean;
    ComposeHeadRotation(clean, 0.0f, 30.0f, 0.0f, /*worldSpaceYaw=*/true, &tracked);
    Check(tracked.Yaw != clean.Yaw, "the head yaw did reach the rotator");
    float ruf[3];
    ResolveAimInTrackedView(clean, tracked, ruf);
    CheckNear(ruf[0], 0.0f, "world-space yaw looking down leaves the aim's right offset at zero");
    CheckNear(ruf[1], 0.0f, "and its up offset");
    CheckNear(ruf[2], 1.0f, "with the aim still dead ahead");
}

// ---------------------------------------------------------------------------
// aim_projection.h - viewport pixels for the published aim vector.
// ---------------------------------------------------------------------------

void AMisreadViewportIsRefused() {
    Check(IsUsableViewport(1920.0f, 1080.0f), "an ordinary resolution is usable");
    Check(!IsUsableViewport(0.0f, 1080.0f), "a zero width is a mis-read");
    Check(!IsUsableViewport(1920.0f, 32.0f), "and so is a height below any display");
    Check(!IsUsableViewport(99999.0f, 1080.0f), "and so is one above any display");
    Check(!IsUsableViewport(kNan, 1080.0f), "and so is a non-finite one");
}

void AnUnconstrainedCameraGetsTheWholeViewport() {
    const ViewRect r = ComputeViewRect(1920.0f, 1080.0f, 0.0f);
    CheckNear(r.x, 0.0f, "no constraint leaves the rect at the viewport origin");
    CheckNear(r.y, 0.0f, "in y too");
    CheckNear(r.w, 1920.0f, "with the full width");
    CheckNear(r.h, 1080.0f, "and the full height");

    // Believing an absurd ratio collapses the rect to a sliver and pins the crosshair in
    // a band across the middle of the screen.
    const ViewRect absurd = ComputeViewRect(1920.0f, 1080.0f, 100.0f);
    CheckNear(absurd.h, 1080.0f, "a ratio outside any real constrained view is disbelieved");
}

void AConstrainedCameraTakesFromTheLongerSide() {
    const ViewRect wide = ComputeViewRect(1920.0f, 1080.0f, 2.39f);
    CheckNear(wide.w, 1920.0f, "a wider constrained ratio keeps the width");
    CheckNear(wide.h, 1920.0f / 2.39f, "and takes height", 0.01f);
    CheckNear(wide.y, (1080.0f - 1920.0f / 2.39f) * 0.5f, "centring what is left", 0.01f);

    const ViewRect narrow = ComputeViewRect(1920.0f, 1080.0f, 1.0f);
    CheckNear(narrow.h, 1080.0f, "a narrower one keeps the height");
    CheckNear(narrow.w, 1080.0f, "and takes width");
    CheckNear(narrow.x, (1920.0f - 1080.0f) * 0.5f, "centring what is left");
}

void TheAimProjectsToTheCentreWhenItIsDeadAhead() {
    const ViewRect rect = { 0.0f, 0.0f, 1920.0f, 1080.0f };
    AimPixel p = {};
    Check(ProjectAimToPixels(rect, 90.0f, 0.0f, 0.0f, 1.0f, &p),
          "an aim straight ahead projects");
    CheckNear(p.x, 960.0f, "onto the horizontal centre of the rect");
    CheckNear(p.y, 540.0f, "and the vertical centre");
    CheckNear(p.ndc_x, 0.0f, "with zero horizontal NDC");
    CheckNear(p.ndc_y, 0.0f, "and zero vertical NDC");
    Check(!p.clamped, "and nothing to pin");
}

void AnAimOffTheFrameIsPinnedRatherThanRefused() {
    const ViewRect rect = { 0.0f, 0.0f, 1920.0f, 1080.0f };
    AimPixel p = {};
    // Refusing would walk the crosshair back to screen centre, where it asserts the shot
    // lands dead ahead while it lands up to 180 degrees away.
    Check(ProjectAimToPixels(rect, 90.0f, 100.0f, 0.0f, 0.01f, &p),
          "an aim swung off the frame still projects");
    Check(p.clamped, "and reports that it was pinned");
    const float inset = 1080.0f * kEdgeInsetFraction;
    CheckNear(p.x, 1920.0f - inset, "onto the inset right edge");

    Check(ProjectAimToPixels(rect, 90.0f, -100.0f, 0.0f, -1.0f, &p),
          "an aim behind the view projects to an edge too");
    CheckNear(p.x, inset, "on the side it points to");
}

void TheProjectionRefusesOnlyWhenThereIsNoDirectionToPointAt() {
    const ViewRect rect = { 0.0f, 0.0f, 1920.0f, 1080.0f };
    AimPixel p = {};
    Check(!ProjectAimToPixels(rect, 0.0f, 0.0f, 0.0f, 1.0f, &p),
          "a field of view outside the usable range refuses");
    Check(!ProjectAimToPixels(rect, 200.0f, 0.0f, 0.0f, 1.0f, &p),
          "and so does one above it");
    Check(!ProjectAimToPixels(rect, 90.0f, kNan, 0.0f, 1.0f, &p),
          "a non-finite aim component refuses");
    // Exactly behind is PINNED, not refused. Refusing leaves the HUD easing the crosshair
    // back to viewport centre, where it claims the shot lands dead ahead while it lands
    // exactly backward - the worst disagreement the projection can produce.
    Check(ProjectAimToPixels(rect, 90.0f, 0.0f, 0.0f, -1.0f, &p),
          "an aim exactly behind the view is pinned rather than refused");
    Check(p.clamped, "and it is reported as pinned");
    CheckNear(p.x, rect.w * 0.5f, "at the horizontal centre", 1.0f);
    Check(p.y > rect.h * 0.5f, "and against the bottom edge");
}

void TheInsetComesFromTheShorterSide() {
    // Taking it from the height alone inverted the horizontal bounds on a rect narrower
    // than two insets and placed the crosshair outside the rect.
    const ViewRect tall = { 0.0f, 0.0f, 200.0f, 4000.0f };
    AimPixel p = {};
    Check(ProjectAimToPixels(tall, 90.0f, 1000.0f, 0.0f, 0.01f, &p),
          "a very tall rect still projects");
    Check(p.x >= tall.x && p.x <= tall.x + tall.w,
          "and the pinned position stays inside it");
    Check(p.y >= tall.y && p.y <= tall.y + tall.h, "in y as well");
}

}  // namespace

int main() {
    SanitizersRejectOnlyWhatTheyMust();
    AMissingIniIsCreatedWithTheShippedDefaults();
    ABadValueLandsOnTheDefaultRatherThanReachingTheCamera();
    ThePortIsTheOneErrorThatRefusesToStart();
    TheAdsModeSurvivesARestart();

    AnUnreadablePageIsRejectedEvenWhenItIsCommitted();

    RotatorUnitsConvertTheWayTheEngineDoes();
    ThePitchBoundLimitsTheHeadsContributionNotTheSum();
    ComposingAZeroHeadPoseLeavesTheViewpointAlone();
    WorldSpaceYawIsPlainRotatorAddition();
    TheAimResolvesToStraightAheadWhenTheHeadIsCentred();
    CleanRollNeverReachesTheAimDirection();
    APointResolvesAgainstTheRenderEye();
    TheShippedProfileHasItsOffsetsInTheRightFields();
    RewritingTheMatrixWithItsOwnPoseChangesNothing();
    TheRewrittenMatrixProjectsFromTheTrackedView();
    TheRewrittenMatrixKeepsTheProjectionTerms();
    ALeanMovesTheAnchorsWithTheEye();
    ADegenerateMatrixIsLeftAlone();
    HeadRollAloneLeavesTheAimAtScreenCentre();
    HeadPitchAloneMovesTheAimVerticallyOnly();
    HeadPitchAndRollRotateTheOffsetWithoutResizingIt();
    WorldSpaceYawLookingStraightDownLeavesTheAimAtCentre();

    TheZoomFactorIsOneUnlessTheGameIsZoomingIn();

    TheLeanTraceReachesPastTheLean();
    AGlancingHitStopsTheLeanFurtherBack();

    AnUnusableValueIsRefusedRatherThanQuietlyReplaced();

    AMisreadViewportIsRefused();
    AnUnconstrainedCameraGetsTheWholeViewport();
    AConstrainedCameraTakesFromTheLongerSide();
    TheAimProjectsToTheCentreWhenItIsDeadAhead();
    AnAimOffTheFrameIsPinnedRatherThanRefused();
    TheProjectionRefusesOnlyWhenThereIsNoDirectionToPointAt();
    TheInsetComesFromTheShorterSide();

    if (g_failures != 0) {
        std::printf("%d check(s) failed\n", g_failures);
        return 1;
    }
    std::printf("Thief unit tests passed\n");
    return 0;
}
