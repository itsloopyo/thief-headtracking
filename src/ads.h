// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "cameraunlock/ads/ads_fade.h"

namespace ThiefHeadTracking {

// Head tracking carries straight on while the bow is drawn. Rotation is never faded, made
// relative or suspended: turning the rendered camera about the eye leaves the arrow's line
// through the eye, so the bow comes up off to one side with its aim still on the crosshair.
//
// The lean is the one thing the draw changes. It translates the rendered eye off the line
// the arrow leaves along, and this mod has no weapon pass of its own to draw the bow from
// the clean eye, so in sights locked the lean is eased out while the bow is drawn and back
// in when it comes down. True free look keeps the lean in full: the bow stays put and the
// eye moves around it. Game thread only, like the rest of the camera hook.
class LeanEase {
public:
    // @p aiming is the bow-draw state polled from the game this frame. Returns the scale
    // for x, y and z: 1 at the hip or in true free look, 0 with the bow drawn in sights
    // locked, smoothstepped between. Switching mode mid-draw rides the same fade.
    float Update(bool aiming, bool trueFreeLook, unsigned long long nowMs) {
        return m_fade.Update(aiming && !trueFreeLook, nowMs);
    }

    // Called on every frame no lean is applied for a reason that is not the bow: menu,
    // loading, master toggle, tracker dropout. The next draw then starts from the hip.
    void Reset() { m_fade.Reset(); }

private:
    cameraunlock::ads::AdsFade m_fade;
};

}  // namespace ThiefHeadTracking
