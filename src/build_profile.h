// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "cameraunlock/memory/pe_fingerprint.h"

#include <cstdint>

namespace ThiefHeadTracking {

// The game module every RVA in a profile is relative to, and the module the fingerprint
// is read from.
//
// Steam launches Binaries\Win32\Shipping-ThiefGame.exe, an 83 KB stub whose only job is
// to spawn ..\..\Binaries2\Win32\Shipping-ThiefGame.exe. That second process is the
// LAUNCHER, and on a 64-bit Windows it reads its own Run32Bit setting - off by default -
// and then spawns ..\Win64\Shipping-ThiefGame.exe -nolauncher and exits. So the game runs
// in the 64-bit executable, which is the one this mod is built against.
//
// All four executables carry the same module name, which is one reason the profile is
// matched on the PE fingerprint rather than on the name: the launcher would otherwise be
// mistaken for the game.
constexpr const char* kGameExeName = "Shipping-ThiefGame.exe";

// One shipped Thief build: its PE fingerprint and the RVAs and struct offsets the hooks
// pin to. Append-only registry (see AGENTS.md "Maintain compatibility across new
// patches"): a patch that moves RVAs gets a NEW profile added to the top of
// kKnownProfiles, never an in-place edit, so users on older builds keep matching their
// original profile by fingerprint.
struct BuildProfile {
    const char* name;
    cameraunlock::memory::PeFingerprint fingerprint;

    // APlayerController::GetPlayerViewPoint(FVector* outLoc, FRotator* outRot),
    // __thiscall, ret 8. Reads the camera POV cache and is the shared "where is the
    // player looking" accessor: the renderer, weapon aim, interaction traces, AI and
    // audio all come through it.
    std::uintptr_t rvaGetPlayerViewPoint;

    // ULocalPlayer::CalcSceneView(FSceneViewFamily*, FVector& outViewLocation,
    // FRotator& outViewRotation, FViewport*, FViewElementDrawer*) -> FSceneView*. Hooked
    // only to say which of its own callers is asking for a view.
    std::uintptr_t rvaCalcSceneView;

    // Return address of the ONE call to GetPlayerViewPoint inside
    // ULocalPlayer::CalcSceneView. Injecting only for this caller is what decouples look
    // from aim: the scene view gets the head-tracked viewpoint, every other caller keeps
    // the one the mouse chose.
    std::uintptr_t rvaCalcSceneViewReturn;

    // CalcSceneView's own callers all reach GetPlayerViewPoint through that same single
    // call, so the return address above cannot tell them apart. Only the viewport draw
    // should be head-tracked. These are the return addresses of the callers that must not
    // be: ULocalPlayer::DeProject, which turns a screen position into a world ray, and
    // the streaming-only branch of FViewport::Draw, which only wants to know where to
    // prefetch textures from. Zero means the build has no such caller to exclude.
    std::uintptr_t rvaDeProjectCaller;
    std::uintptr_t rvaStreamingCaller;

    // APlayerController::GetFOVAngle, (AController*) -> float degrees. It is a thin
    // wrapper that calls the script event through ProcessEvent, and CalcSceneView calls it
    // two instructions after the viewpoint to build the projection matrix - so it is the
    // field of view the frame is actually rendered with, rather than a POV-cache field
    // read at a guessed offset that would miss LockedFOV and every camera modifier.
    std::uintptr_t rvaGetFovAngle;

    // APlayerController::PlayerCamera, and the camera's DefaultFOV: what this camera
    // renders at when nothing is zooming it, which is the base the zoom compensation
    // measures against. CalcSceneView reads the same pair to turn the FOV into a LOD
    // distance factor.
    std::uint32_t offPlayerCamera;
    std::uint32_t offCamDefaultFov;

    // The trace wrapper called by AActor::execTrace, shared by aim and lean queries.
    std::uintptr_t rvaGWorld;
    std::uintptr_t rvaWorldTrace;

    // TRACE_World for this build, read off the game's own script trace native rather than
    // reconstructed. Which flags level geometry blocks is a project setting.
    std::uint32_t traceWorldFlags;

    // The gameplay gate.
    //
    // rvaGEngine points at the UEngine* global; offEngineTransitionType is UEngine's
    // TransitionType byte, which the engine sets while loading, saving, connecting or
    // paused and clears back to TT_None in play. UWorld::PersistentLevel and the level's
    // actor array give AWorldInfo, whose Pauser is non-null exactly while the game is
    // paused.
    std::uintptr_t rvaGEngine;
    std::uint32_t offEngineTransitionType;
    std::uint32_t offWorldPersistentLevel;
    std::uint32_t offLevelActors;
    std::uint32_t offWorldInfoPauser;

    // GetViewTarget feeds the view-target log line, which shows when a cutscene takes the view.
    std::uint32_t vtGetViewTarget;
    std::uint32_t offControllerPawn;

    // HUD projection storage, accessors, and the UI callers allowed a tracked copy.
    // The accessors are pinned by address, not read out of a live camera's vtable: the
    // front end's menu camera has nothing in those slots, so a vtable read depends on which
    // camera happens to be live. Each accessor appears in exactly one vtable in the image (AThiefCamera's,
    // at +0x8E8 / +0x8F8 / +0x950), so hooking the function is the same as hooking the slot.
    std::uint32_t offCamHudBasis;
    std::uintptr_t rvaCameraHudMatrix;
    std::uintptr_t rvaCameraHudProject;
    std::uintptr_t rvaCameraForward;
    std::uintptr_t rvaHudMatrixCaller;
    std::uintptr_t rvaHudForwardCaller;

    // The bow aim - "marksman" mode - which is Thief's aim-down-sights state: the aim
    // camera takes the view onto the arrow's line, the field of view narrows, and
    // UThiefUIMarksmanCrosshairs is the crosshair on screen.
    //
    // offPawnMarksman is the
    // UThiefMarksmanComponent the player pawn holds; the game reaches it off the pawn at
    // one fixed offset from three places, two of which publish the Marksman_bActive /
    // Marksman_eState / Marksman_fDrawRatio animation keys and the third of which is the
    // marksman-aim focus distance. offMarksmanAimBits and maskMarksmanAim are the flag
    // word on that component and the bit that is set for as long as the bow is drawn.
    //
    // Zero in any of the four leaves ADS detection off, and the mod then reports "not
    // aiming" on every frame. That is the safe direction: the lean stays in rather than
    // being eased out on a frame that is not an aim.
    std::uint32_t offPawnMarksman;
    std::uint32_t offMarksmanAimBits;
    std::uint32_t maskMarksmanAim;

    std::uintptr_t rvaGameInstance;
    std::uint32_t offGameUI;
    std::uint32_t vtUIGetWidget;
    std::uint32_t offMarksmanWidget;
    std::uintptr_t rvaGfxSetPosition;
    std::uintptr_t rvaGfxGetVisibleFrameRect;
    std::uint32_t offGfxMovie;
    std::uint32_t offSceneProjection;
};

// Most-recent build first (diagnostic primary).
extern const BuildProfile kKnownProfiles[];
extern const int kKnownProfileCount;

// Returns the profile matching the running EXE, or nullptr when no profile matches (mod
// stays dormant - no hooks installed).
const BuildProfile* MatchRunningProfile();

}  // namespace ThiefHeadTracking
