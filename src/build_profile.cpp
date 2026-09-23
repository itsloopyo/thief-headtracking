// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "build_profile.h"

#include "logging.h"

#include <windows.h>

namespace ThiefHeadTracking {

// steam-win64-20141007: the shipped Steam build, Binaries2\Win64\Shipping-ThiefGame.exe,
// 37,675,520 bytes, TimeDateStamp 0x5433C73C, SizeOfImage 0x029F2000, CheckSum
// 0x023FB77B, ImageBase 0x140000000. The launcher reports it as "1.7 build 4158.21".
//
// Every RVA below was read out of that image. Where a number is 0 the feature it belongs
// to is switched off rather than guessed at - see the block after the profile.
//
// FUN_14039a2c0 (RVA 0x39A2C0) is AController::eventGetPlayerViewPoint: it copies the two
// out-params into a script parameter block, resolves NAME_GetPlayerViewPoint through
// FindFunctionChecked and calls ProcessEvent (vtable +0x2C8), then copies the results
// back. It is the shared "where is the player looking" accessor - the renderer, weapon
// aim, interaction traces, AI and audio all reach the viewpoint through it, which is why
// injecting for exactly one caller is what decouples look from aim.
//
// FUN_1403f6500 (RVA 0x3F6500) is ULocalPlayer::CalcSceneView. It reads the viewport size
// off the FViewport vtable, calls the viewpoint accessor once at 0x1403F66BE (returning to
// RVA 0x3F66C3), then GetFOVAngle, then builds the axis-swap and FPerspectiveMatrix that
// the frame is drawn with. The LOD line two statements later is what names the camera
// offsets: it reads PlayerCamera at controller+0x450 and that camera's DefaultFOV at
// +0x2A4, falling back to the controller's own DefaultFOV at +0x4AC, and writes the
// distance factor to +0x4B0.
//
// CalcSceneView has three callers, and all three reach the viewpoint through that same
// single call, so the return address alone cannot tell them apart:
//   RVA 0x3F8941  UGameViewportClient::Draw   <- the render. Inject here.
//   RVA 0x3F74F5  ULocalPlayer::DeProject     <- screen position to world ray.
//   RVA 0x2CAE91  the third caller            <- not the render either.
// DeProject is the one that matters: handing it a head-rotated view would let a
// screen-to-world query answer with where the player is LOOKING rather than where the game
// is aiming, which is the coupling this hook exists to remove.
//
// FUN_1403f0c70 (RVA 0x3F0C70) is APlayerController::GetFOVAngle, and CalcSceneView calls
// it two instructions after the viewpoint. Calling it rather than reading a POV-cache
// field is what makes the reticle's field of view the one the projection matrix was built
// from, LockedFOV and every camera modifier included.
//
// DAT_1420fc8e0 (RVA 0x20FC8E0) is GWorld, named by FUN_14050a9a0(GWorld, 0) =
// UWorld::GetWorldInfo, which walks world+0x80 -> +0x60 -> [0]. That walk is what the pause
// gate uses.
//
// DAT_1420f96e0 (RVA 0x20F96E0) is GEngine, named by UGameViewportClient::Draw iterating
// its GamePlayers array at +0x74C / +0x754.
//
// AController::Pawn is at +0x294, read off the controller's replication list
// (FUN_1402a8540). That function resolves the Engine package's "Pawn" and
// "PlayerReplicationInfo" properties by name and, for each, compares the field between the
// live actor and the last-sent copy: `MOV RDX,[R12 + 0x294]` / `MOV RCX,[RBP + 0x294]` for
// Pawn and +0x29C for PlayerReplicationInfo. Those two are exactly AController's replicated
// properties, so the pairing is the engine's own rather than an inference. Both reads are
// qword and neither offset is 8-aligned, which is what the script compiler's property
// layout produces here - the disassembly is the authority, not the alignment.
//
// AThiefCamera caches the HUD's world-to-clip matrix at +0xAF0, and its Update is the
// virtual at vtable +0x8C0. The matrix was named by reading the routine at 0xBE7F30, which
// transforms a world point as v * M - so its columns are the view axes - and confirmed
// against the clean rotator in game: at clean pitch -29.17 / yaw -45.87 the right column
// read (0.7181, 0.6959, 0.0007) against a horizon-locked (-sin y, cos y, 0) of
// (0.7175, 0.6967, 0), and the forward column (0.6072, -0.6261, -0.4871) against
// (0.6084, -0.6265, -0.4876). The camera that holds it is the controller's PlayerCamera:
// controller+0x720 and controller+0x450 were logged as the same object.
//
// APlayerController::GetViewTarget() is the virtual at vtable +0x8F8. Its script thunk
// (FUN_1402ffb70, the exec side of intAPlayerControllerexecGetViewTarget) does nothing but
// call that slot and copy the result into the return value, so the slot is read rather than
// counted, and no ACamera struct offset has to be guessed to reach the view target.
static const BuildProfile kSteamProfile_20141007 = {
    "steam-win64-20141007",
    { 0x5433C73Cu, 0x029F2000u, 0x023FB77Bu },
    0x39A2C0u,   // rvaGetPlayerViewPoint
    0x3F6500u,   // rvaCalcSceneView
    0x3F66C3u,   // rvaCalcSceneViewReturn
    0x3F74F5u,   // rvaDeProjectCaller
    0x2CAE91u,   // rvaStreamingCaller
    0x3F0C70u,   // rvaGetFovAngle
    0x450u,      // offPlayerCamera
    0x2A4u,      // offCamDefaultFov
    0x20FC8E0u,  // rvaGWorld
    0x396FE0u,   // rvaWorldTrace
    0x2086u,     // traceWorldFlags
    0x20F96E0u,  // rvaGEngine
    0u,          // offEngineTransitionType - not pinned, see below
    0x80u,       // offWorldPersistentLevel
    0x60u,       // offLevelActors
    0x8F0u,      // offWorldInfoPauser
    0x8F8u,      // vtGetViewTarget
    0x294u,      // offControllerPawn
    0xAF0u,      // offCamHudBasis
    0xBE7F00u,   // rvaCameraHudMatrix
    0xBE7F30u,   // rvaCameraHudProject
    0xBEB290u,   // rvaCameraForward
    0xBC7F58u,   // rvaHudMatrixCaller
    0xBC8209u,   // rvaHudForwardCaller
    0u,          // offPawnMarksman
    0u,          // offMarksmanAimBits
    0u,          // maskMarksmanAim
    0x21FC200u,  // rvaGameInstance
    0xAB8u,      // offGameUI
    0x390u,      // vtUIGetWidget
    0xA8u,       // offMarksmanWidget
    0x794EA0u,   // rvaGfxSetPosition
    0x77BD70u,   // rvaGfxGetVisibleFrameRect
    0x40u,       // offGfxMovie
    0x110u,      // offSceneProjection
};

// The zeros above, and what each one costs. A zero switches its feature off and the
// startup log says so; none of them is a guess left in place to be discovered in game.
//
// offEngineTransitionType - the byte UEngine sets while a map loads, a save is written or
//   the engine connects. The only candidate found in the image is a property-change
//   callback that reads a TransitionType at +0x60 of an object that could not be shown to
//   be GEngine, and believing it would have read the engine's vtable pointer and reported
//   every frame of the session as a transition. The pause gate covers the pause menu; a
//   loading screen is covered by there being no PlayerCamera to render through. Finish it
//   with the struct probe: dump GEngine in the main menu and in play and diff.
//
// offPawnMarksman / offMarksmanAimBits / maskMarksmanAim - the bow draw. The component is
//   reached off the player pawn at one fixed offset by three functions (RVA 0x8AB160,
//   0x8AB530, 0xC0E740) and its flag word is at component+0xC0 with at least two live
//   bits, but which bit is "the bow is drawn" was not settled from the binary. With these
//   at zero the mod reports "not aiming" every frame and the lean is never eased out for
//   the draw, which is the safe direction. offControllerPawn is pinned now, so finishing
//   the ADS pin is the three component offsets and nothing else.
//
const BuildProfile kKnownProfiles[] = {
    kSteamProfile_20141007,
};
const int kKnownProfileCount =
    static_cast<int>(sizeof(kKnownProfiles) / sizeof(kKnownProfiles[0]));

const BuildProfile* MatchRunningProfile() {
    HMODULE hExe = GetModuleHandleA(kGameExeName);
    if (!hExe) {
        Log::Line("ERROR: %s module not found for fingerprinting", kGameExeName);
        return nullptr;
    }

    cameraunlock::memory::PeFingerprint running{};
    if (!cameraunlock::memory::ReadPeFingerprint(hExe, running)) {
        Log::Line("ERROR: could not read PE fingerprint of %s", kGameExeName);
        return nullptr;
    }

    for (int i = 0; i < kKnownProfileCount; ++i) {
        if (running.Matches(kKnownProfiles[i].fingerprint)) {
            Log::Line("Build profile matched: %s", kKnownProfiles[i].name);
            return &kKnownProfiles[i];
        }
    }

    using cameraunlock::memory::ClassifyMismatch;
    using cameraunlock::memory::FingerprintMismatch;
    const BuildProfile& primary = kKnownProfiles[0];
    switch (ClassifyMismatch(running, primary.fingerprint)) {
        case FingerprintMismatch::Newer:
            Log::Line("Unrecognised Thief build (newer than %s). Check the releases page "
                      "for an updated mod. Staying dormant.", primary.name);
            break;
        case FingerprintMismatch::Older:
            Log::Line("Unrecognised Thief build (older than %s). Let Steam finish "
                      "updating. Staying dormant.", primary.name);
            break;
        case FingerprintMismatch::Differs:
            Log::Line("This Shipping-ThiefGame.exe is not the 64-bit game build this mod "
                      "knows: the launcher's own 32-bit process carries the same name, and "
                      "so does a tampered or repacked executable. Staying dormant.");
            break;
    }
    return nullptr;
}

}  // namespace ThiefHeadTracking
