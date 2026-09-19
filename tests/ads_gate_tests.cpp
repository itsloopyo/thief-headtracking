// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo
//
// The gate half of the ADS behaviour: which frames head tracking stands down on while the
// bow is drawn, what the log is told closed the gate, and how the setting is read and
// cycled.
//
// Thief is a two-slot mod, so `marker` is not one of its modes. The parse test is what
// keeps that true for a config file written by a three-slot sibling.

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

void CheckNear(float actual, float expected, const char* what) {
    Check(std::fabs(actual - expected) <= 1e-3f, what);
}

using ThiefHeadTracking::AdsController;
using ThiefHeadTracking::AdsFrame;
using ThiefHeadTracking::AdsMode;
using ThiefHeadTracking::AdsPose;
using ThiefHeadTracking::ApplyAdsToGate;
using ThiefHeadTracking::AdvanceAdsMode;
using ThiefHeadTracking::ParseAdsMode;
using ThiefHeadTracking::kGateAds;

constexpr unsigned long long kLowerMs = cameraunlock::ads::AdsFade::kLowerMs;
constexpr unsigned long long kRaiseMs = cameraunlock::ads::AdsFade::kRaiseMs;

AdsPose Head(float pitch) {
    AdsPose p;
    p.pitch = pitch;
    return p;
}

// Paused closes the gate on the sights, and names the sights as the reason.
void PausedClosesTheGateWithItsOwnReason() {
    const std::uint32_t bits = ApplyAdsToGate(true, AdsMode::Paused, /*poseGone=*/true);
    Check(bits == kGateAds, "paused closes the gate with the ADS reason");
}

// The tracked mode never closes it: that is the whole difference between the two slots.
void TrackedKeepsTheGateOpen() {
    const std::uint32_t bits = ApplyAdsToGate(true, AdsMode::Tracked, true);
    Check(bits == 0, "tracked keeps the gate open through the aim");
}

// A bow that is not drawn never closes the gate, whichever mode is selected.
void ABowThatIsDownNeverClosesTheGate() {
    Check(ApplyAdsToGate(false, AdsMode::Paused, true) == 0,
          "paused leaves the gate open while the bow is down");
    Check(ApplyAdsToGate(false, AdsMode::Tracked, true) == 0,
          "and so does tracked");
}

// The gate stays open for the length of the transition, then closes behind it. Closing on
// the first aiming frame would hand the camera back mid-fade, which is the one-frame cut the
// fade exists to remove.
void PausedHoldsTheGateOpenUntilThePoseHasGone() {
    AdsController ads;
    const AdsPose p = Head(20.0f);

    AdsFrame f = ads.Update(AdsMode::Paused, true, true, p, 1000);
    Check(ApplyAdsToGate(true, AdsMode::Paused, f.pose_gone) == 0,
          "the gate is still open on the frame the bow comes up");

    f = ads.Update(AdsMode::Paused, true, true, p, 1000 + kLowerMs / 2);
    Check(ApplyAdsToGate(true, AdsMode::Paused, f.pose_gone) == 0,
          "and still open half way through the transition");

    f = ads.Update(AdsMode::Paused, true, true, p, 1000 + kLowerMs);
    Check(ApplyAdsToGate(true, AdsMode::Paused, f.pose_gone) == kGateAds,
          "the gate closes once the transition has run the pose down");
}

// The state is polled, so it heals on its own. Nothing has to deliver an exit edge for the
// gate to reopen - which matters, because Thief drives the bow from an animation state
// machine that can leave the aim without one.
void TheStateHealsWithoutAnExitEdge() {
    AdsController ads;
    const AdsPose p = Head(20.0f);
    ads.Update(AdsMode::Paused, true, true, p, 1000);
    AdsFrame f = ads.Update(AdsMode::Paused, true, true, p, 1000 + kLowerMs);
    Check(f.pose_gone, "the controller has run the pose all the way down");
    Check(ApplyAdsToGate(true, AdsMode::Paused, f.pose_gone) == kGateAds,
          "so the gate is closed while the bow is drawn");

    Check(f.pose.pitch == 0.0f, "and the head pose it applies has gone to nothing");

    // The next poll simply says the bow is down. The assertion that matters is on the
    // CONTROLLER'S POSE, not on the gate: ApplyAdsToGate returns 0 for a lowered bow
    // whatever the controller did, so asserting only the gate would still pass with a fade
    // that never leaves the aim - and head tracking would then never come back after the
    // first draw, in every session, with this suite green.
    const unsigned long long released = 1000 + kLowerMs + 16;
    f = ads.Update(AdsMode::Paused, false, true, p, released);
    Check(ApplyAdsToGate(true, AdsMode::Paused, f.pose_gone) == 0,
          "the gate reopens even if the sights were still reported up");

    // The ride out starts on that frame, so it has covered nothing yet. One frame later it
    // must be under way, and by the end of the raise it must be all the way home.
    f = ads.Update(AdsMode::Paused, false, true, p, released + 16);
    Check(f.pose.pitch > 0.0f, "and the head pose is climbing back a frame later");
    Check(f.pose.pitch < p.pitch, "still short of the full pose while it rides out");

    f = ads.Update(AdsMode::Paused, false, true, p, released + kRaiseMs);
    CheckNear(f.pose.pitch, p.pitch, "and all the way back once the ride out is over");
}

// The cycle is two slots. A third that only cycles back to the first is worse than none:
// the player presses the key expecting a change and gets one that does nothing.
void TheCycleIsTwoSlots() {
    Check(AdvanceAdsMode(AdsMode::Paused) == AdsMode::Tracked, "paused cycles to tracked");
    Check(AdvanceAdsMode(AdsMode::Tracked) == AdsMode::Paused, "tracked cycles back to paused");
}

// Anything the mod does not have is the default, not whichever branch happens to be last.
// `marker` is in that set: it is a three-slot mod's mode, and a config carrying it must land
// on stock ADS rather than on head tracking through the sights nobody asked for.
void AnUnknownValueLandsOnTheDefault() {
    Check(ParseAdsMode("paused") == AdsMode::Paused, "paused parses");
    Check(ParseAdsMode("tracked") == AdsMode::Tracked, "tracked parses");
    Check(ParseAdsMode("  TRACKED  ") == AdsMode::Tracked, "spacing and case are forgiven");
    Check(ParseAdsMode("marker") == AdsMode::Paused, "marker is not a mode this mod has");
    Check(ParseAdsMode("banana") == AdsMode::Paused, "a typo lands on the default");
    Check(ParseAdsMode("") == AdsMode::Paused, "an empty value lands on the default");
    Check(ParseAdsMode(nullptr) == AdsMode::Paused, "a missing value lands on the default");
    Check(cameraunlock::ads::kDefaultAdsMode == AdsMode::Paused, "the default is paused");
}

}  // namespace

int main() {
    PausedClosesTheGateWithItsOwnReason();
    TrackedKeepsTheGateOpen();
    ABowThatIsDownNeverClosesTheGate();
    PausedHoldsTheGateOpenUntilThePoseHasGone();
    TheStateHealsWithoutAnExitEdge();
    TheCycleIsTwoSlots();
    AnUnknownValueLandsOnTheDefault();

    if (g_failures != 0) {
        std::printf("%d ADS gate check(s) failed\n", g_failures);
        return 1;
    }
    std::printf("ADS gate tests passed\n");
    return 0;
}
