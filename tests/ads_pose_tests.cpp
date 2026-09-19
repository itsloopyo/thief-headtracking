// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo
//
// The pose half of the ADS behaviour: what the transition feeds the camera while the bow is
// drawn, and what it feeds on the way back out.
//
// None of this is reachable from a settings or a gate test, and every rule here has a bug
// behind it: a yaw delta taken across the -180/180 seam whips the view a full turn the wrong
// way, an entry pose captured off a stale rotation holds the whole aim at that offset, and a
// roll made relative levels a head tilt the player is actively holding.

#include "ads.h"

#include <cmath>
#include <cstdio>

namespace {

int g_failures = 0;

void Check(bool ok, const char* what) {
    if (!ok) {
        ++g_failures;
        std::printf("FAIL: %s\n", what);
    }
}

void CheckNear(float got, float want, const char* what) {
    if (!(std::fabs(got - want) < 1e-3f)) {
        ++g_failures;
        std::printf("FAIL: %s (got %.4f, want %.4f)\n", what, got, want);
    }
}

using ThiefHeadTracking::AdsController;
using ThiefHeadTracking::AdsFrame;
using ThiefHeadTracking::AdsMode;
using ThiefHeadTracking::AdsPose;

AdsPose Pose(float pitch, float yaw, float roll, float x = 0.0f, float y = 0.0f,
             float z = 0.0f) {
    AdsPose p;
    p.pitch = pitch;
    p.yaw = yaw;
    p.roll = roll;
    p.x = x;
    p.y = y;
    p.z = z;
    return p;
}

constexpr unsigned long long kLowerMs = cameraunlock::ads::AdsFade::kLowerMs;
constexpr unsigned long long kRaiseMs = cameraunlock::ads::AdsFade::kRaiseMs;

// Hip fire hands the absolute pose straight through, untouched, in both modes.
void HipFireIsUntouched() {
    for (const AdsMode mode : { AdsMode::Paused, AdsMode::Tracked }) {
        AdsController ads;
        const AdsFrame f = ads.Update(mode, /*aiming=*/false, /*live=*/true,
                                      Pose(10.0f, 20.0f, 5.0f, 0.1f, 0.2f, 0.3f), 1000);
        CheckNear(f.pose.pitch, 10.0f, "hip fire keeps the pitch");
        CheckNear(f.pose.yaw, 20.0f, "hip fire keeps the yaw");
        CheckNear(f.pose.roll, 5.0f, "hip fire keeps the roll");
        CheckNear(f.pose.x, 0.1f, "hip fire keeps the lean");
        Check(!f.pose_gone, "nothing is suppressed at the hip");
    }
}

// The entry frame itself is still the full pose: the transition leaves at rest rather than
// stepping, which is what makes it a fade and not a switch.
void EntryFrameIsFullPose() {
    AdsController ads;
    const AdsFrame f = ads.Update(AdsMode::Paused, true, true, Pose(10.0f, 20.0f, 5.0f), 1000);
    CheckNear(f.pose.pitch, 10.0f, "paused entry frame is the full pitch");
    CheckNear(f.pose.yaw, 20.0f, "paused entry frame is the full yaw");
    Check(!f.pose_gone, "the pose is not gone on the entry frame");
}

// Paused runs the pose down to nothing over kLowerMs and holds it there.
void PausedFadesToNothing() {
    AdsController ads;
    const AdsPose p = Pose(10.0f, 20.0f, 5.0f, 0.1f, 0.2f, 0.3f);
    ads.Update(AdsMode::Paused, true, true, p, 1000);

    const AdsFrame mid = ads.Update(AdsMode::Paused, true, true, p, 1000 + kLowerMs / 2);
    Check(std::fabs(mid.pose.pitch) < 10.0f && std::fabs(mid.pose.pitch) > 0.0f,
          "paused is part way through the fade at the half-way point");
    Check(!mid.pose_gone, "the gate cannot close while the fade is still running");

    const AdsFrame done = ads.Update(AdsMode::Paused, true, true, p, 1000 + kLowerMs);
    CheckNear(done.pose.pitch, 0.0f, "paused ends on no pitch");
    CheckNear(done.pose.yaw, 0.0f, "paused ends on no yaw");
    CheckNear(done.pose.x, 0.0f, "paused ends on no lean");
    Check(done.pose_gone, "the pose is gone once the fade has run");

    // The lean rides the same fade rather than being cut at the edge: the arrow leaves the
    // bow on the muzzle line, so an eye offset from it moves the sight picture off target.
    AdsController leanOnly;
    leanOnly.Update(AdsMode::Paused, true, true, p, 1000);
    const AdsFrame midLean = leanOnly.Update(AdsMode::Paused, true, true, p,
                                             1000 + kLowerMs / 2);
    Check(std::fabs(midLean.pose.x) > 0.0f && std::fabs(midLean.pose.x) < 0.1f,
          "the lean fades with the rotation instead of being cut");
}

// Roll is in neither fade. A head tilt moves no aim point, so zeroing it levels a tilt the
// player is holding and leans it back in on the way out: two horizon jolts per aim.
void RollIsNeverFaded() {
    for (const AdsMode mode : { AdsMode::Paused, AdsMode::Tracked }) {
        AdsController ads;
        const AdsPose p = Pose(10.0f, 20.0f, 7.0f);
        ads.Update(mode, true, true, p, 1000);
        const AdsFrame done = ads.Update(mode, true, true, p, 1000 + kLowerMs);
        CheckNear(done.pose.roll, 7.0f, "roll survives the fade at full strength");

        // And it stays absolute as the head keeps rolling through the aim.
        const AdsFrame later =
            ads.Update(mode, true, true, Pose(10.0f, 20.0f, -12.0f), 1000 + kLowerMs + 16);
        CheckNear(later.pose.roll, -12.0f, "roll is never measured from the entry frame");
    }
}

// A tracked aim arrives at the same place a paused one does - the entry pose is identity -
// and tracks on from there.
void TrackedIsRelativeToEntry() {
    AdsController ads;
    ads.Update(AdsMode::Tracked, true, true, Pose(10.0f, 20.0f, 0.0f, 0.1f, 0.0f, 0.0f), 1000);

    const AdsFrame settled = ads.Update(AdsMode::Tracked, true, true,
                                        Pose(10.0f, 20.0f, 0.0f, 0.1f, 0.0f, 0.0f),
                                        1000 + kLowerMs);
    CheckNear(settled.pose.pitch, 0.0f, "a held head arrives at the aim, not beside it");
    CheckNear(settled.pose.yaw, 0.0f, "a held head arrives at the aim in yaw too");
    CheckNear(settled.pose.x, 0.0f, "the lean is measured from the entry frame as well");

    const AdsFrame moved = ads.Update(AdsMode::Tracked, true, true,
                                      Pose(15.0f, 25.0f, 0.0f, 0.3f, 0.0f, 0.0f),
                                      1000 + kLowerMs + 16);
    CheckNear(moved.pose.pitch, 5.0f, "tracking carries on from the aim");
    CheckNear(moved.pose.yaw, 5.0f, "tracking carries on in yaw");
    CheckNear(moved.pose.x, 0.2f, "and in position");
}

// Yaw arrives wrapped into -180..180, so the delta has to be the short way round. A plain
// subtraction reads this 20 degree move as -340 and whips the view a full turn.
void YawCrossesTheSeamTheShortWay() {
    AdsController ads;
    ads.Update(AdsMode::Tracked, true, true, Pose(0.0f, 170.0f, 0.0f), 1000);
    const AdsFrame f = ads.Update(AdsMode::Tracked, true, true, Pose(0.0f, -170.0f, 0.0f),
                                  1000 + kLowerMs);
    CheckNear(f.pose.yaw, 20.0f, "a yaw across the seam is the short way round");
}

// The entry pose is captured from a LIVE rotation. Interpolators return nothing on a
// suppressed frame, and capturing then freezes a pre-suppression pose for the whole aim.
void EntryCaptureWaitsForALiveRotation() {
    AdsController ads;
    // The bow comes up on a frame with no fresh sample: the pose passes through untouched
    // rather than becoming the entry frame.
    const AdsFrame stale = ads.Update(AdsMode::Tracked, true, /*live=*/false,
                                      Pose(40.0f, 0.0f, 0.0f), 1000);
    CheckNear(stale.pose.pitch, 40.0f, "a dead frame is not captured as the entry pose");

    // The first live frame is the entry, and settles to identity.
    ads.Update(AdsMode::Tracked, true, /*live=*/true, Pose(10.0f, 0.0f, 0.0f), 1016);
    const AdsFrame settled = ads.Update(AdsMode::Tracked, true, true, Pose(10.0f, 0.0f, 0.0f),
                                        1016 + kLowerMs);
    CheckNear(settled.pose.pitch, 0.0f, "the first live frame is the entry pose");
}

// Lowering the bow hands back the absolute pose, so the view swings back by exactly the
// angle the head is holding. Dropping the entry pose any earlier would make the relative
// pose the absolute one and step the view by the whole offset in a single frame.
void LoweringReturnsTheAbsolutePose() {
    AdsController ads;
    ads.Update(AdsMode::Tracked, true, true, Pose(10.0f, 0.0f, 0.0f), 1000);
    ads.Update(AdsMode::Tracked, true, true, Pose(30.0f, 0.0f, 0.0f), 1000 + kLowerMs);

    const AdsFrame out = ads.Update(AdsMode::Tracked, false, true, Pose(30.0f, 0.0f, 0.0f),
                                    1000 + kLowerMs + kRaiseMs);
    CheckNear(out.pose.pitch, 30.0f, "the ride out ends on the absolute pose");
    Check(!out.pose_gone, "nothing is suppressed once the bow is down");
}

// A tap of the aim button is the most common input there is. Each leg starts from where the
// transition actually is, so a tap does not remove a fully applied pose in one frame.
void AReversalStartsFromWhereItIs() {
    AdsController ads;
    const AdsPose p = Pose(20.0f, 0.0f, 0.0f);
    ads.Update(AdsMode::Paused, true, true, p, 1000);
    const AdsFrame downABit = ads.Update(AdsMode::Paused, true, true, p, 1000 + kLowerMs / 3);
    Check(downABit.pose.pitch < 20.0f && downABit.pose.pitch > 0.0f,
          "the fade is part way down when the button is released");

    // Released on the same instant: the pose is exactly where the lowering leg had reached.
    // An implementation that started each leg at its own endpoint would report 0 here and
    // remove the whole remaining pose in one frame.
    const AdsFrame released = ads.Update(AdsMode::Paused, false, true, p, 1000 + kLowerMs / 3);
    CheckNear(released.pose.pitch, downABit.pose.pitch,
              "releasing resumes from where the fade had reached");

    // And it climbs back from there rather than jumping.
    const AdsFrame climbing =
        ads.Update(AdsMode::Paused, false, true, p, 1000 + kLowerMs / 3 + 16);
    Check(climbing.pose.pitch > released.pose.pitch, "the ride back out has started");
    Check(climbing.pose.pitch <= 20.0f, "and has not overshot the pose the head is holding");
}

// Every suppression that is not the sights drops the entry pose, so the next aim re-enters
// cleanly rather than resuming against a pose from before the menu.
void ResetDropsTheEntryPose() {
    AdsController ads;
    ads.Update(AdsMode::Tracked, true, true, Pose(10.0f, 0.0f, 0.0f), 1000);
    ads.Reset();

    // Still aiming, but the entry pose is gone: this frame becomes the new entry.
    ads.Update(AdsMode::Tracked, true, true, Pose(25.0f, 0.0f, 0.0f), 2000);
    const AdsFrame settled = ads.Update(AdsMode::Tracked, true, true, Pose(25.0f, 0.0f, 0.0f),
                                        2000 + kLowerMs);
    CheckNear(settled.pose.pitch, 0.0f, "the aim re-enters on the pose it came back with");
}

}  // namespace

int main() {
    HipFireIsUntouched();
    EntryFrameIsFullPose();
    PausedFadesToNothing();
    RollIsNeverFaded();
    TrackedIsRelativeToEntry();
    YawCrossesTheSeamTheShortWay();
    EntryCaptureWaitsForALiveRotation();
    LoweringReturnsTheAbsolutePose();
    AReversalStartsFromWhereItIs();
    ResetDropsTheEntryPose();

    if (g_failures != 0) {
        std::printf("%d ADS pose check(s) failed\n", g_failures);
        return 1;
    }
    std::printf("ADS pose tests passed\n");
    return 0;
}
