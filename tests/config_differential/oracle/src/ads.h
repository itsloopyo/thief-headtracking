// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "game_state.h"

#include "cameraunlock/ads/ads_blend.h"
#include "cameraunlock/ads/ads_fade.h"
#include "cameraunlock/ads/ads_mode.h"
#include "cameraunlock/ads/entry_pose.h"

#include <cstdint>

namespace ThiefHeadTracking {

// What head tracking does while the bow is drawn.
//
// Thief ships TWO slots, not three. The three-slot shape exists for a game that hides its
// crosshair in the aim and leaves the player reading iron sights or an optic, where the mod
// owes them a marker of its own. Thief has exactly one crosshair class in the whole binary,
// UThiefUIMarksmanCrosshairs, and it is the bow-aim one: there is no hip-fire reticle for
// it to be an ADS variant of. So the reticle this mod already moves onto the impact point
// IS the aim indicator, and it is alive precisely while the sights are up. `marker` is
// therefore not a mode here - not in the cycle, not in the INI, not in the README.
using cameraunlock::ads::AdsMode;
using AdsPose = cameraunlock::ads::AdsEntryPose::Pose;

// Named apart from core's own NextAdsMode: both take an AdsMode, so an unqualified
// call would find core's by argument-dependent lookup and be ambiguous with this one.
inline AdsMode AdvanceAdsMode(AdsMode mode) {
    return cameraunlock::ads::NextAdsModeTwoSlot(mode);
}

// `marker` is refused rather than accepted-and-ignored: a config written by a three-slot
// sibling mod, or by a future release of this one, would otherwise select a mode that does
// not exist here. It lands on the default instead.
inline AdsMode ParseAdsMode(const char* text) {
    return cameraunlock::ads::ParseAdsMode(text, /*allowMarker=*/false);
}

struct AdsFrame {
    // This frame's head pose after the ADS transition: faded to nothing in `paused`, and
    // measured from the frame the bow came up on in `tracked`.
    AdsPose pose;
    // The transition has run the head pose all the way down. Only then can the gate close
    // on the sights, because closing it earlier hands the camera back mid-fade and cuts
    // the pose in one frame, which is the jolt the fade exists to remove.
    bool pose_gone = false;
};

// The per-frame ADS decision. Game thread only, like the rest of the camera hook.
//
// Both halves are core's: AdsFade owns the shape of the transition, AdsEntryPose owns the
// relative pose the tracked mode feeds. Neither is written again here - this only wires
// them to the frame.
class AdsController {
public:
    // @p aiming is the polled ADS state for this frame, taken from the game rather than
    // from the gate verdict. In `paused` the fade is what CLOSES that gate, so feeding the
    // verdict back in would make the fade start raising the instant it finished lowering.
    //
    // @p live says a real sample arrived on either channel this frame, rather than the
    // nothing a suppressed frame publishes. Capturing the entry pose from a stale frame
    // holds the whole aim at that offset.
    //
    // Either channel, not rotation alone: position-only tracking mode publishes no rotation
    // by design, so a rotation-only test never captures an entry pose there and a tracked
    // aim carries the player's whole standing lean into the sights. The cost of the wider
    // test is a frame that has position but a rejected rotation, which captures a zero
    // rotation entry - one aim entered without its rotational snap, against a mode that
    // otherwise never works at all.
    AdsFrame Update(AdsMode mode, bool aiming, bool live, const AdsPose& absolute,
                    unsigned long long nowMs) {
        const float scale = m_fade.Update(aiming, nowMs);
        const AdsPose relative = m_entry.Relative(aiming, live, absolute);

        AdsFrame out;
        out.pose = cameraunlock::ads::BlendAdsPose(mode, scale, absolute, relative);
        out.pose_gone = aiming && scale <= 0.0f;
        return out;
    }

    // Called on every frame tracking is suppressed for a reason that is NOT the sights:
    // menu, loading, cinematic, master toggle, tracker dropout. The ADS suppression is the
    // one the transition must survive, because it closed that gate itself.
    void Reset() {
        m_fade.Reset();
        m_entry.Reset();
    }

private:
    cameraunlock::ads::AdsFade m_fade;
    cameraunlock::ads::AdsEntryPose m_entry;
};

// The ADS branch of the verdict walk, and deliberately the LAST step of it.
//
// Reached only on a frame the game-state gate left open. A menu, a loading screen or a
// cinematic outranks the sights, and does so by returning before this: it keeps its own
// reason in the log and drops the transition, so there is nothing left here for it to
// outrank. @p aiming is the polled sights state on such a frame.
//
// Closes the gate only in `paused`, and only once the transition has run the head pose all
// the way down - the mode says whether the sights suspend tracking, @p poseGone says the
// ride is over. Handing the camera back mid-fade is the one-frame cut the fade removes.
inline std::uint32_t ApplyAdsToGate(bool aiming, AdsMode mode, bool poseGone) {
    if (aiming && cameraunlock::ads::AdsSuspendsTracking(mode) && poseGone) {
        return kGateAds;
    }
    return 0;
}

}  // namespace ThiefHeadTracking
