// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "aim_trace.h"

#include "world_trace.h"

namespace ThiefHeadTracking {

AimPoint TraceAim(const UE3Vector& eye, const UE3Rotator& cleanRot, const void* sourceActor) {
    AimPoint out;

    // Ahead of the direction, not after it. With no trace pinned for this build the answer
    // is "could not run" whatever the aim is, and this fires from the render path on every
    // frame the head is off centre - four trig calls to reach a foregone conclusion.
    if (!WorldTraceReady()) {
        return out;
    }

    float dir[3];
    RotatorForward(cleanRot, dir);
    const float start[3] = { eye.X, eye.Y, eye.Z };

    // AActor::execTrace uses 0x8020BF when actors are included, excluding its source pawn.
    const TraceHit hit = TraceWorld(start, dir, kAimTraceRange, sourceActor, 0x8020BFu);
    if (!hit.queried) {
        return out;
    }
    out.queried = true;
    if (!hit.blocked) {
        return out;
    }

    out.hit = true;
    // TraceWorld already measured the distance the only way it can be trusted - the
    // contact's own world position projected onto the ray, rather than a field labelled
    // Distance by inspection - and rejected an answer that did not land on the ray it
    // cast. Recomputing it here would be the same arithmetic without that range check.
    out.point = UE3Vector{ hit.point[0], hit.point[1], hit.point[2] };
    out.distance = hit.distance;
    return out;
}

}  // namespace ThiefHeadTracking
