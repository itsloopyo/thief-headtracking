// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "game_state.h"

#include "camera_diagnostics.h"
#include "logging.h"
#include "memory_probe.h"
#include "struct_probe.h"

#include <cstring>

namespace ThiefHeadTracking {

namespace {

// UEngine::TransitionType. TT_None is play; everything else is a screen the player is
// looking at rather than through - the engine sets it while a map loads, while a save is
// written, while it connects, while it precaches, and while it is paused.
constexpr std::uint8_t kTransitionNone = 0;

std::uintptr_t g_gengine = 0;
std::uintptr_t g_gworld = 0;
std::uint32_t g_offTransitionType = 0;
std::uint32_t g_offPersistentLevel = 0;
std::uint32_t g_offLevelActors = 0;
std::uint32_t g_offPauser = 0;
std::uint32_t g_offControllerPawn = 0;
std::uint32_t g_vtGetViewTarget = 0;
std::uint32_t g_offPawnMarksman = 0;
std::uint32_t g_offMarksmanAimBits = 0;
std::uint32_t g_maskMarksmanAim = 0;

// How much of AWorldInfo the probe dumps when diagnostics are on. Wide enough to reach
// the Pauser offset the gate reads, which is what a session re-pins after a patch.
constexpr std::uint32_t kWorldInfoProbeBytes = 0x400u;

std::uintptr_t Deref(std::uintptr_t p) {
    return *reinterpret_cast<const std::uintptr_t*>(p);
}

// One hop of a pointer walk: reject the address, then follow it. Every walk below is
// built out of this, so no hop can be written without its check.
bool Follow(std::uintptr_t p, std::uintptr_t* out) {
    if (!Readable(p, sizeof(std::uintptr_t))) {
        return false;
    }
    *out = Deref(p);
    return true;
}

// One hop from a field the script compiler did not put on an 8 boundary.
//
// AController::Pawn is at +0x294 and the engine loads it with a plain unaligned qword, so
// Follow cannot read it: that helper validates the ADDRESS as 8-aligned, which is the right
// test for an object header and the wrong one for a property the layout placed on a 4
// boundary. Rejected there, the pawn read answers "no pawn" on every frame of the session,
// which shuts the gameplay gate for good and leaves the mod doing nothing in game.
//
// So the address is only checked for readability, and the 8-alignment test moves onto the
// VALUE, which is where it catches what it is for: a pointer read while it was half
// written. A null field is a real answer - the controller possesses nothing - and is
// reported as one rather than as a failed read.
bool FollowField(std::uintptr_t p, std::uintptr_t* out) {
    if (!ReadableSpan(p, sizeof(std::uintptr_t))) {
        return false;
    }
    std::uintptr_t value = 0;
    std::memcpy(&value, reinterpret_cast<const void*>(p), sizeof(value));
    if (value != 0 && !Readable(value, sizeof(std::uintptr_t))) {
        return false;
    }
    *out = value;
    return true;
}

// The actor the controller is possessing, or nullptr when it possesses nothing.
//
// A controller with no Pawn is not a player standing in a level: it is the front end, the
// menu map behind it, the gap while the next map streams in, and the frames either side of
// a death. Every one of those still builds a scene view, which is why head tracking ran in
// them.
const std::uint8_t* ControllerPawn(const std::uint8_t* controller) {
    std::uintptr_t pawn = 0;
    if (!FollowField(reinterpret_cast<std::uintptr_t>(controller) + g_offControllerPawn,
                     &pawn)) {
        return nullptr;
    }
    return reinterpret_cast<const std::uint8_t*>(pawn);
}

// The accessor handles pending targets and returns the controller when no target exists.
const std::uint8_t* ViewTarget(const std::uint8_t* controller) {
    using GetViewTarget_t = void*(__fastcall*)(const void* self);
    std::uintptr_t vtable = 0;
    if (!Follow(reinterpret_cast<std::uintptr_t>(controller), &vtable) ||
        !Readable(vtable + g_vtGetViewTarget, sizeof(std::uintptr_t))) {
        return nullptr;
    }
    const auto fn = *reinterpret_cast<GetViewTarget_t*>(vtable + g_vtGetViewTarget);
    if (fn == nullptr) {
        return nullptr;
    }
    return static_cast<const std::uint8_t*>(fn(controller));
}

// AWorldInfo is always the first entry of the persistent level's actor array, which is
// how the engine's own GetWorldInfo() reaches it.
const std::uint8_t* WorldInfo() {
    std::uintptr_t world = 0, level = 0, actors = 0, info = 0;
    if (!Follow(g_gworld, &world) ||
        !Follow(world + g_offPersistentLevel, &level) ||
        !Follow(level + g_offLevelActors, &actors) ||
        !Follow(actors, &info) ||
        !Readable(info + g_offPauser, sizeof(std::uintptr_t))) {
        return nullptr;
    }
    return reinterpret_cast<const std::uint8_t*>(info);
}

// Whether the bow is drawn, walked from the controller the scene view asked the viewpoint
// of: controller -> Pawn -> UThiefMarksmanComponent -> its flag word.
//
// POLLED, never latched. Thief drives the marksman state from an animation state machine
// that can transition without an event reaching us - firing and re-drawing inside one
// state, a takedown interrupting the draw - and a latched flag that missed one edge would
// either strand the player in ADS behaviour or leak hip-fire tracking into the aim.
//
// Every failure answers "not aiming". An unreadable frame that answered "still aiming"
// would suspend tracking on a frame that is not an aim, with nothing on screen to explain
// it; failing toward stock is the safe direction, and each hop is validated because a level
// load rebuilds the objects behind these pointers while the render path is still reading
// them.
bool ReadMarksmanAiming(const std::uint8_t* controller) {
    // All FOUR, because the walk uses all four and they are pinned by separate steps of the
    // same hunt. With offControllerPawn left at 0 the first hop reads the controller's own
    // vtable pointer as the Pawn and the walk carries on through read-only data, answering
    // from a constant - which in `paused` suspends tracking for the whole session while the
    // startup line says ads=on. Every hop below is Readable-checked, so nothing faults; it
    // just reports a fixed answer, which is worse.
    if (g_maskMarksmanAim == 0 || g_offPawnMarksman == 0 || g_offControllerPawn == 0 ||
        g_offMarksmanAimBits == 0 || controller == nullptr) {
        return false;
    }
    const auto self = reinterpret_cast<std::uintptr_t>(controller);
    std::uintptr_t pawn = 0, marksman = 0;
    if (!FollowField(self + g_offControllerPawn, &pawn) || pawn == 0 ||
        !Follow(pawn + g_offPawnMarksman, &marksman) ||
        !ReadableSpan(marksman + g_offMarksmanAimBits, sizeof(std::uint32_t))) {
        return false;
    }
    const std::uint32_t bits =
        *reinterpret_cast<const std::uint32_t*>(marksman + g_offMarksmanAimBits);
    return (bits & g_maskMarksmanAim) != 0;
}

}  // namespace

void InitGameState(const BuildProfile& profile, std::uintptr_t moduleBase) {
    g_gengine = profile.rvaGEngine ? moduleBase + profile.rvaGEngine : 0;
    g_gworld = profile.rvaGWorld ? moduleBase + profile.rvaGWorld : 0;
    g_offTransitionType = profile.offEngineTransitionType;
    g_offPersistentLevel = profile.offWorldPersistentLevel;
    g_offLevelActors = profile.offLevelActors;
    g_offPauser = profile.offWorldInfoPauser;
    g_offControllerPawn = profile.offControllerPawn;
    g_vtGetViewTarget = profile.vtGetViewTarget;
    g_offPawnMarksman = profile.offPawnMarksman;
    g_offMarksmanAimBits = profile.offMarksmanAimBits;
    g_maskMarksmanAim = profile.maskMarksmanAim;

    // ads=OFF is not cosmetic: it means the mod cannot see the bow being drawn, so the ADS
    // mode does nothing whichever way it is set. A player reporting that head tracking
    // carries on through the bow has their answer on this line.
    // Each column is the SAME expression its gate branches on. Derived separately they
    // drift, and a column that reads "on" for a gate that is standing down is worse than no
    // column at all - it is the one line a triage session trusts.
    Log::Line("Gameplay gate: transition=%s pause=%s pawn=%s cutscenes=tracked ads=%s",
              g_gengine && g_offTransitionType ? "on" : "OFF",
              g_gworld && g_offPersistentLevel && g_offLevelActors && g_offPauser
                      ? "on" : "OFF",
              g_offControllerPawn ? "on" : "OFF",
              g_maskMarksmanAim && g_offPawnMarksman && g_offControllerPawn &&
                      g_offMarksmanAimBits ? "on" : "OFF");
}

std::uint32_t ReadGameStateGate(const std::uint8_t* controller, bool* outAiming) {
    // Set before anything can return, so no caller reads a flag left over from a previous
    // frame.
    *outAiming = false;

    std::uint32_t bits = 0;

    // An unpinned TransitionType offset would read the engine's own vtable pointer, which
    // is never zero, and report every frame of the session as a transition. The offset has
    // to be pinned for the check to run at all.
    // ReadableSpan, not Readable: TransitionType is a one-byte struct field, and Readable
    // additionally demands an 8-aligned address because it is the test for an object
    // pointer. A field that happens to land off an 8 boundary - seven addresses in eight -
    // would fail it on every frame, leaving this gate permanently shut off while the
    // startup line still reports transition=on.
    std::uintptr_t engine = 0;
    if (g_gengine && g_offTransitionType && Follow(g_gengine, &engine) &&
        ReadableSpan(engine + g_offTransitionType, 1)) {
        const std::uint8_t transition =
            *reinterpret_cast<const std::uint8_t*>(engine + g_offTransitionType);
        if (transition != kTransitionNone) {
            bits |= kGateTransition;
        }
    }

    // Every offset the walk dereferences, not just the last one. WorldInfo hops through
    // PersistentLevel and the level's actor array to reach AWorldInfo, so a profile that
    // pinned Pauser alone would walk world+0 - the engine's own vtable pointer - and the
    // startup line would still read pause=on.
    if (g_gworld && g_offPersistentLevel && g_offLevelActors && g_offPauser) {
        const std::uint8_t* info = WorldInfo();
        if (info != nullptr) {
            ProbeStruct("worldinfo", info, kWorldInfoProbeBytes);
            if (Deref(reinterpret_cast<std::uintptr_t>(info) + g_offPauser) != 0) {
                bits |= kGatePaused;
            }
        }
    }

    if (g_offControllerPawn && controller != nullptr) {
        const std::uint8_t* pawn = ControllerPawn(controller);
        const std::uint8_t* target = g_vtGetViewTarget ? ViewTarget(controller) : nullptr;
        // The front end's menu camera is a view target with no pawn behind it. Every
        // in-level cutscene seen so far keeps the pawn possessed while a scripted camera
        // holds the view, so no pawn is not play whatever the view target is.
        if (pawn == nullptr) {
            bits |= kGateNoPawn;
        }
        ReportViewTarget(pawn, target);
    }

    *outAiming = ReadMarksmanAiming(controller);
    return bits;
}

}  // namespace ThiefHeadTracking
