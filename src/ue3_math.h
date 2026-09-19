// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "cameraunlock/math/angle_utils.h"

#include <cmath>
#include <cstdint>

namespace ThiefHeadTracking {

// UE3 POD camera types (32-bit engine: FVector is 3 floats, FRotator is 3 int32
// where 65536 units = 360 degrees). Do NOT use the core ue_math.h FVector/FRotator
// here - those are UE5 doubles and would mis-decode this engine's out-params.
struct UE3Vector {
    float X, Y, Z;
};
struct UE3Rotator {
    std::int32_t Pitch, Yaw, Roll;
};

// A full turn of an FRotator component. Every conversion below is derived from it rather
// than from a repeated 65536 / 32768 literal, so the engine's unit is stated once.
constexpr float kRotatorUnitsPerTurn = 65536.0f;

constexpr float kUnitsToRad =
    static_cast<float>(2.0 * cameraunlock::math::kPi / kRotatorUnitsPerTurn);
constexpr float kDegToRad   = static_cast<float>(cameraunlock::math::kPi / 180.0);
// For reporting a rotator in degrees. The hook never converts this way to drive the
// camera - it is the log that has to be readable.
constexpr float kUnitsToDeg = 360.0f / kRotatorUnitsPerTurn;

// UE3 keeps a view pitch inside a quarter turn. Past it cos(pitch) goes negative, which
// mirrors the forward axis's horizontal part and inverts the up axis, so the view snaps
// upside-down and backwards. The engine does not renormalise what the camera hook hands
// back, so the sum has to be clamped.
constexpr std::int32_t kMaxPitchUnits =
    static_cast<std::int32_t>(kRotatorUnitsPerTurn) / 4;

// An FRotator field reduced to the signed half-turn range [-32768, 32767].
//
// A rotator is modular, so the engine is free to hand back either representation and
// UE3 produces both: FMatrix::Rotator() returns signed, while the positive-wrapped form
// (a 30 degree downward look as 60000 rather than -5536) comes out of the view-limiting
// path. Every other use the mod makes of the rotator is periodic and cannot tell them
// apart - RotatorToMatrix is trig, MatrixToRotator re-derives a signed value - but the
// pitch clamp compares against a bound, and against the positive-wrapped form it would
// read every downward look as far past the limit and pin the view at the sky.
//
// Writing the normalised value back is safe for the same reason: the out-param feeds
// FRotationMatrix, which is modular.
inline std::int32_t NormalizeUnits(std::int32_t units) {
    return static_cast<std::int16_t>(static_cast<std::uint16_t>(units));
}

// An FRotator component as signed degrees, for the log. Never used to drive the camera.
inline float UnitsToDegrees(std::int32_t units) {
    return static_cast<float>(NormalizeUnits(units)) * kUnitsToDeg;
}

// Every float that reaches an FRotator or an FVector out-param has to be finite.
// DegToUnits truncates to int32 and the position offset is added straight into the
// camera location, so one non-finite component parks the view at a coordinate the
// renderer cannot resolve: a black screen, with nothing in the log to say why. The
// pose is checked once, where it leaves the tracking pipeline.
inline bool AllFinite(float a, float b, float c) {
    return std::isfinite(a) && std::isfinite(b) && std::isfinite(c);
}

// @p deg must be finite (see AllFinite).
//
// A rotator is modular, so reducing by whole turns is exact and only ever applied to a
// value that would otherwise overflow the conversion. That matters because an INI
// sensitivity is finite-checked but deliberately NOT magnitude-checked - a large one is
// legitimate tuning - and a big enough product puts lround's result outside int32,
// where the narrowing conversion is undefined rather than merely wrong. Below the
// threshold nothing is reduced, so a full turn still converts to a full turn's units.
inline std::int32_t DegToUnits(float deg) {
    constexpr float kMaxDirectDegrees = kRotatorUnitsPerTurn * 0.5f;
    const float reduced = (deg > -kMaxDirectDegrees && deg < kMaxDirectDegrees)
                              ? deg
                              : std::fmod(deg, 360.0f);
    return static_cast<std::int32_t>(
        std::lround(reduced * kRotatorUnitsPerTurn / 360.0f));
}

inline std::int32_t RadToUnits(float rad) {
    return static_cast<std::int32_t>(
        std::lround(rad * (kRotatorUnitsPerTurn * 0.5) / cameraunlock::math::kPi));
}

inline std::int32_t ClampPitchUnits(std::int32_t pitch) {
    return cameraunlock::math::Clamp(pitch, -kMaxPitchUnits, kMaxPitchUnits);
}

// UE3 FRotationMatrix: row 0 is the forward (X) axis, row 1 right (Y), row 2 up (Z),
// composed as M(P,Y,R) = M(P,0,R) * M(0,Y,0). Yaw being the outermost rotation about
// world Z is why plain FRotator addition gives horizon-locked yaw, and why camera-local
// yaw needs this matrix path.
struct Mat3 {
    float m[3][3];
};

inline Mat3 RotatorToMatrix(float pitchRad, float yawRad, float rollRad) {
    const float sp = std::sin(pitchRad), cp = std::cos(pitchRad);
    const float sy = std::sin(yawRad),   cy = std::cos(yawRad);
    const float sr = std::sin(rollRad),  cr = std::cos(rollRad);
    Mat3 M;
    M.m[0][0] = cp * cy;
    M.m[0][1] = cp * sy;
    M.m[0][2] = sp;
    M.m[1][0] = sr * sp * cy - cr * sy;
    M.m[1][1] = sr * sp * sy + cr * cy;
    M.m[1][2] = -sr * cp;
    M.m[2][0] = -(cr * sp * cy + sr * sy);
    M.m[2][1] = sr * cy - cr * sp * sy;
    M.m[2][2] = cr * cp;
    return M;
}

inline Mat3 RotatorToMatrix(const UE3Rotator& r) {
    return RotatorToMatrix(static_cast<float>(r.Pitch) * kUnitsToRad,
                           static_cast<float>(r.Yaw) * kUnitsToRad,
                           static_cast<float>(r.Roll) * kUnitsToRad);
}

// The forward (X) axis on its own. It is row 0 of RotatorToMatrix, which carries no
// roll term at all, so a caller that only needs "where is this rotator pointing" pays
// four trig calls instead of six and skips building the two rows it would discard.
inline void RotatorForward(const UE3Rotator& r, float out[3]) {
    const float pitchRad = static_cast<float>(r.Pitch) * kUnitsToRad;
    const float yawRad   = static_cast<float>(r.Yaw) * kUnitsToRad;
    const float cp = std::cos(pitchRad);
    out[0] = cp * std::cos(yawRad);
    out[1] = cp * std::sin(yawRad);
    out[2] = std::sin(pitchRad);
}

inline Mat3 MatMul(const Mat3& a, const Mat3& b) {
    Mat3 r;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            r.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] + a.m[i][2] * b.m[2][j];
        }
    }
    return r;
}

// How much of @p headPitchDeg the view has room for, in FRotator units, given the
// engine's current pitch.
//
// Past a quarter turn cos(pitch) goes negative, which mirrors the forward axis and
// inverts the up axis: the view snaps upside-down and backwards. Bounding the head's
// CONTRIBUTION rather than the final sum is what lets both yaw modes share one limit -
// the camera-local branch composes rather than adds, and clamping its result after the
// fact cannot undo a flip that has already thrown yaw and roll half a turn out.
inline std::int32_t BoundedPitchContribution(std::int32_t cleanPitch, float headPitchDeg) {
    const std::int32_t normalized = NormalizeUnits(cleanPitch);
    return ClampPitchUnits(normalized + DegToUnits(headPitchDeg)) - normalized;
}

// Components of @p v along the axes of the view @p t describes: right, up, forward.
// Rows 1, 2 and 0 of an FRotationMatrix ARE those three axes, so the resolve is three
// dot products and nothing else.
inline void ResolveInView(const float v[3], const Mat3& t, float outRuf[3]) {
    outRuf[0] = v[0] * t.m[1][0] + v[1] * t.m[1][1] + v[2] * t.m[1][2];
    outRuf[1] = v[0] * t.m[2][0] + v[1] * t.m[2][1] + v[2] * t.m[2][2];
    outRuf[2] = v[0] * t.m[0][0] + v[1] * t.m[0][1] + v[2] * t.m[0][2];
}

// Where the CLEAN aim direction lands in the TRACKED view, as components along that
// view's own right/up/forward axes. This is the whole of the reticle projection's
// geometry, and it is deliberately resolved basis-to-basis rather than through an Euler
// formula: the tracked rotator is the same one handed to the engine, so the crosshair
// cannot drift out of agreement with the render however the rotation was composed.
//
// Only the clean FORWARD axis is needed, and row 0 carries no roll term, so the clean
// roll never reaches the result.
inline void ResolveAimInTrackedView(const UE3Rotator& clean, const UE3Rotator& tracked,
                                    float outRuf[3]) {
    float f[3];
    RotatorForward(clean, f);
    ResolveInView(f, RotatorToMatrix(tracked), outRuf);
}

// Where a WORLD POINT lands in the tracked view, as components along that view's own
// right/up/forward axes, seen from @p renderEye.
//
// This is the 6DOF form of the projection and it is what the reticle uses whenever the
// aim trace found a surface. A direction is enough only while the rendered eye and the
// eye the shot leaves from are the same point; a lean separates them by up to 30 cm, and
// from there the fixed impact point is no longer straight ahead. The vector is therefore
// taken from the RENDER eye to the impact point rather than from the shot eye, which is
// what makes the reticle stay on the thing it marks at every range instead of only at
// one.
inline void ResolvePointInTrackedView(const UE3Vector& renderEye, const UE3Vector& point,
                                      const UE3Rotator& tracked, float outRuf[3]) {
    const float v[3] = { point.X - renderEye.X, point.Y - renderEye.Y,
                         point.Z - renderEye.Z };
    ResolveInView(v, RotatorToMatrix(tracked), outRuf);
}

// UE3 FMatrix::Rotator(): pitch/yaw from the forward axis, roll from the
// right/up axes projected onto the roll-free right axis.
inline void MatrixToRotator(const Mat3& M, UE3Rotator* out) {
    const float fx = M.m[0][0], fy = M.m[0][1], fz = M.m[0][2];
    const float pitch = std::atan2(fz, std::sqrt(fx * fx + fy * fy));
    const float yaw   = std::atan2(fy, fx);
    const float syx = -std::sin(yaw), syy = std::cos(yaw);
    const float roll  = std::atan2(M.m[2][0] * syx + M.m[2][1] * syy,
                                   M.m[1][0] * syx + M.m[1][1] * syy);
    out->Pitch = RadToUnits(pitch);
    out->Yaw   = RadToUnits(yaw);
    out->Roll  = RadToUnits(roll);
}

// Adds a tracker-convention head pose to the game's viewpoint rotator.
//
// This is the engine boundary for rotation, and the whole of it: the roll sign, the
// quarter-turn pitch bound, and the two yaw modes. It lives here rather than inside the
// detour so it can be exercised without a running game - the bug it exists to prevent
// was at the call site, not in the helpers it calls.
//
// Yaw and pitch are inherited from dishonored-headtracking's ue3_math.h rather than
// re-derived. That mod hooks the same UE3 function and receives the same FRotator
// out-param in the same units, and its signs were settled in a running game, so both
// reach the engine unchanged. Re-deriving them from handedness would have been a coin
// flip per axis - the tracker states no convention, so the engine alone cannot answer it.
//
// Roll is negated, and that came from playing this game rather than from the ancestor:
// tilting the head one way rolled the view the other. It is done HERE, once, where the
// tracker's convention meets the engine's, rather than by asking the player to set
// InvertRoll - that key is for a tracker that genuinely reports an axis backwards, and
// spending it on a fixed engine convention would leave a user whose tracker IS mirrored
// with no way to say so. The reticle projection resolves against the rotator this
// function writes, so it follows the flipped roll without a second sign of its own.
//
// Yaw: horizon-locked yaw is plain FRotator addition, because UE3 composes yaw outermost
// about world Z. Camera-local yaw needs the matrix path to rotate about the view's own
// axes.
inline void ComposeHeadRotation(const UE3Rotator& clean, float pitchDeg, float yawDeg,
                                float rollDeg, bool worldSpaceYaw, UE3Rotator* rot) {
    const std::int32_t cleanPitch = NormalizeUnits(clean.Pitch);
    const std::int32_t pitchUnits = BoundedPitchContribution(cleanPitch, pitchDeg);

    const float engineRoll = -rollDeg;

    if (worldSpaceYaw) {
        rot->Yaw  += DegToUnits(yawDeg);
        rot->Roll += DegToUnits(engineRoll);
        rot->Pitch = cleanPitch + pitchUnits;
        return;
    }
    const Mat3 cleanM = RotatorToMatrix(clean);
    const Mat3 head = RotatorToMatrix(static_cast<float>(pitchUnits) * kUnitsToRad,
                                      yawDeg * kDegToRad, engineRoll * kDegToRad);
    MatrixToRotator(MatMul(head, cleanM), rot);
}

}  // namespace ThiefHeadTracking
