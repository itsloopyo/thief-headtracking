// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "build_profile.h"
#include "ue3_math.h"

#include <cmath>
#include <cstdint>

namespace ThiefHeadTracking {

// HUD accessors apply the rendered pose when an anchor is projected.
// The camera's cached matrix remains clean between those calls.

// The matrix, in UE3's row-vector convention - a world point is transformed as v * M, so
// the COLUMNS are the view axes and the bottom row is the translation, -dot(eye, axis) per
// column. Column 0 is the camera right, column 1 the up with the projection's vertical term
// already multiplied in, and columns 2 and 3 are the forward, one for depth and one for the
// perspective divide.
//
// Rewrites @p m in place so it describes @p rot instead, seen from the eye it already
// describes plus @p eyeDelta. Each column keeps its own length, so the vertical term and
// whatever separates columns 2 and 3 are carried through untouched.
//
// Writing the matrix's OWN rotation back with a zero delta is an identity. That is what
// makes this safe to run every tick: if the pose is the one the game already chose, nothing
// changes.
//
// Leaves @p m alone when any axis column is degenerate or non-finite, which is what an
// unbuilt matrix on the first frames of a level looks like.
inline void RewriteHudMatrix(float m[16], const UE3Rotator& rot, const float eyeDelta[3]) {
    float axis[4][3];
    float len[4];
    for (int j = 0; j < 4; ++j) {
        const float x = m[j], y = m[4 + j], z = m[8 + j];
        len[j] = std::sqrt(x * x + y * y + z * z);
        // A matrix the engine has not built yet is all zeroes, and one caught mid-rebuild
        // can be worse. Either way there is nothing to rotate and no eye to recover.
        if (!std::isfinite(len[j]) || len[j] <= 1e-6f) {
            return;
        }
        axis[j][0] = x / len[j];
        axis[j][1] = y / len[j];
        axis[j][2] = z / len[j];
    }

    // The eye the matrix already describes. Columns 0..2 are orthonormal once their own
    // lengths are divided out, so the three translations the bottom row carries ARE its
    // coordinates in that frame and the recovery is exact rather than a fit.
    float eye[3] = { 0.0f, 0.0f, 0.0f };
    for (int j = 0; j < 3; ++j) {
        const float d = -m[12 + j] / len[j];
        eye[0] += d * axis[j][0];
        eye[1] += d * axis[j][1];
        eye[2] += d * axis[j][2];
    }
    // The HUD has to project from the eye the frame was DRAWN from, so a positional lean
    // moves the anchors too, not just a turn of the head.
    eye[0] += eyeDelta[0];
    eye[1] += eyeDelta[1];
    eye[2] += eyeDelta[2];

    // RotatorToMatrix puts forward in row 0, right in row 1 and up in row 2, which is the
    // order the columns want.
    const Mat3 t = RotatorToMatrix(rot);
    const float* newAxis[4] = { t.m[1], t.m[2], t.m[0], t.m[0] };

    for (int j = 0; j < 4; ++j) {
        const float* a = newAxis[j];
        m[j]      = a[0] * len[j];
        m[4 + j]  = a[1] * len[j];
        m[8 + j]  = a[2] * len[j];
        m[12 + j] = -(eye[0] * a[0] + eye[1] * a[1] + eye[2] * a[2]) * len[j];
    }
}

// The pose the render path applied this frame, and the camera whose matrix it belongs to.
// Called only for frames that are actually head tracked.
void SetHudPose(const void* camera, const UE3Rotator& tracked, const float eyeDelta[3]);

// No pose this frame - menus, tracking switched off, tracker gone quiet. The
// game's own matrix then stands, which is the un-modded behaviour.
void ClearHudPose();

// Installs the HUD accessor detours. False when any of them failed, which the installer
// has already logged.
bool InstallHudBasisHook(const BuildProfile& profile, std::uintptr_t moduleBase);

}  // namespace ThiefHeadTracking
