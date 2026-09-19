// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include <windows.h>
#include <intrin.h>
#include <cstdio>
#include <cmath>
#include <cstring>

static void* testCaller = nullptr;
#define _ReturnAddress() testCaller
#include "../src/hud_basis.cpp"
#undef _ReturnAddress

namespace ThiefHeadTracking {
bool InstallDetour(std::uintptr_t, void*, void**, const char*) { return true; }
}

namespace {
struct Camera { float matrix[16]; };
void __fastcall ReadMatrix(const void* camera, float* out) {
    std::memcpy(out, static_cast<const Camera*>(camera)->matrix, 64);
}
float* __fastcall ReadForward(const void*, float* out) {
    out[0] = 1; out[1] = 0; out[2] = 0;
    return out;
}
float* __fastcall Project(void* camera, float* out, const float* point) {
    const float* m = static_cast<Camera*>(camera)->matrix;
    const float w = point[0]*m[3] + point[1]*m[7] + point[2]*m[11] + m[15];
    out[0] = (point[0]*m[0] + point[1]*m[4] + point[2]*m[8] + m[12]) / w;
    out[1] = (point[0]*m[1] + point[1]*m[5] + point[2]*m[9] + m[13]) / w;
    return out;
}
}

int main() {
    using namespace ThiefHeadTracking;
    int failures = 0;
    const auto check = [&](bool passed, const char* name) {
        if (!passed) { std::printf("FAIL: %s\n", name); ++failures; }
    };
    const Camera clean = {{0,0,1,1, 1,0,0,0, 0,2,0,0, 0,0,0,0}};
    Camera camera = clean;
    g_origMatrix = &ReadMatrix;
    g_origForward = &ReadForward;
    g_origProject = &Project;
    g_offBasis = 0;
    g_matrixCaller = 1;
    g_forwardCaller = 2;
    const float lean[] = {0,30,0};
    SetHudPose(&camera, {0,DegToUnits(22),0}, lean);
    testCaller = reinterpret_cast<void*>(g_matrixCaller);
    float first[16], second[16];
    MatrixDetour(&camera, first);
    MatrixDetour(&camera, second);
    check(std::memcmp(first, second, 64) == 0, "repeated reads do not accumulate lean");
    check(std::memcmp(first, clean.matrix, 64) != 0, "mission reads receive the tracked matrix");
    check(std::memcmp(camera.matrix, clean.matrix, 64) == 0, "mission reads leave gameplay matrix clean");
    testCaller = nullptr;
    MatrixDetour(&camera, second);
    check(std::memcmp(second, clean.matrix, 64) == 0, "other matrix callers stay clean");
    testCaller = reinterpret_cast<void*>(g_matrixCaller);
    Camera other = clean;
    MatrixDetour(&other, second);
    check(std::memcmp(second, clean.matrix, 64) == 0, "a different camera stays clean");
    float direction[3];
    testCaller = reinterpret_cast<void*>(g_forwardCaller);
    ForwardDetour(&camera, direction);
    check(direction[1] > 0.3f, "off-screen marker direction follows tracked yaw");
    testCaller = nullptr;
    ForwardDetour(&camera, direction);
    check(direction[0] == 1 && direction[1] == 0, "gameplay forward remains clean");
    const float point[] = {400,0,0};
    float xy[2];
    ProjectDetour(&camera, xy, point);
    check(xy[0] < -0.4f, "interaction projection includes yaw and lean");
    check(std::memcmp(camera.matrix, clean.matrix, 64) == 0, "interaction projection restores exact matrix");
    ClearHudPose();
    testCaller = reinterpret_cast<void*>(g_matrixCaller);
    MatrixDetour(&camera, second);
    check(std::memcmp(second, clean.matrix, 64) == 0, "tracking loss clears mission correction");
    ProjectDetour(&camera, xy, point);
    check(xy[0] == 0, "tracking loss clears interaction correction");
    std::printf("HUD accessor tests: %d failures\n", failures);
    return failures ? 1 : 0;
}
