// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "hud_basis.h"
#include "hook_install.h"
#include "logging.h"

#include <windows.h>
#include <intrin.h>
#include <cstring>

namespace ThiefHeadTracking {
namespace {
using Matrix_t = void(__fastcall*)(const void*, float*);
using Project_t = float*(__fastcall*)(void*, float*, const float*);
using Forward_t = float*(__fastcall*)(const void*, float*);
Matrix_t g_origMatrix = nullptr;
Project_t g_origProject = nullptr;
Forward_t g_origForward = nullptr;
std::uint32_t g_offBasis = 0;
std::uintptr_t g_matrixCaller = 0;
std::uintptr_t g_forwardCaller = 0;
const void* g_camera = nullptr;
UE3Rotator g_tracked = {};
float g_eyeDelta[3] = {};
DWORD g_poseTick = 0;

bool PoseIsLive(const void* self) {
    return g_camera == self && g_poseTick != 0 && GetTickCount() - g_poseTick <= 250;
}

void __fastcall MatrixDetour(const void* self, float* out) {
    const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    g_origMatrix(self, out);
    if (caller != g_matrixCaller || !PoseIsLive(self)) return;
    const float cleanRight[3] = { out[0], out[4], out[8] };
    RewriteHudMatrix(out, g_tracked, g_eyeDelta);
    static DWORD lastLog = 0;
    const DWORD now = GetTickCount();
    if (now - lastLog >= 2000) {
        lastLog = now;
        Log::Line("Mission projection: clean right=(%.4f,%.4f,%.4f) tracked right=(%.4f,%.4f,%.4f) pose age=%lu ms",
                  cleanRight[0], cleanRight[1], cleanRight[2], out[0], out[4], out[8], now - g_poseTick);
    }
}

float* __fastcall ForwardDetour(const void* self, float* out) {
    const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    float* result = g_origForward(self, out);
    if (caller == g_forwardCaller && PoseIsLive(self)) {
        const Mat3 basis = RotatorToMatrix(g_tracked);
        std::memcpy(out, basis.m[0], 3 * sizeof(float));
    }
    return result;
}

float* __fastcall ProjectDetour(void* self, float* out, const float* point) {
    if (!PoseIsLive(self)) return g_origProject(self, out, point);
    auto* matrix = reinterpret_cast<float*>(reinterpret_cast<std::uintptr_t>(self) + g_offBasis);
    float clean[16];
    std::memcpy(clean, matrix, sizeof(clean));
    RewriteHudMatrix(matrix, g_tracked, g_eyeDelta);
    // This native projection reads only this matrix and invokes no other game functions.
    float* result = g_origProject(self, out, point);
    std::memcpy(matrix, clean, sizeof(clean));
    return result;
}
}  // namespace

void SetHudPose(const void* camera, const UE3Rotator& tracked, const float eyeDelta[3]) {
    g_camera = camera;
    g_tracked = tracked;
    std::memcpy(g_eyeDelta, eyeDelta, sizeof(g_eyeDelta));
    g_poseTick = GetTickCount();
}

void ClearHudPose() { g_poseTick = 0; }

bool InstallHudBasisHook(const BuildProfile& profile, std::uintptr_t moduleBase) {
    g_offBasis = profile.offCamHudBasis;
    g_matrixCaller = moduleBase + profile.rvaHudMatrixCaller;
    g_forwardCaller = moduleBase + profile.rvaHudForwardCaller;
    if (!InstallDetour(moduleBase + profile.rvaCameraHudMatrix, reinterpret_cast<void*>(&MatrixDetour),
                       reinterpret_cast<void**>(&g_origMatrix), "HUD matrix accessor")) return false;
    if (!InstallDetour(moduleBase + profile.rvaCameraHudProject, reinterpret_cast<void*>(&ProjectDetour),
                       reinterpret_cast<void**>(&g_origProject), "HUD point projection")) return false;
    if (!InstallDetour(moduleBase + profile.rvaCameraForward, reinterpret_cast<void*>(&ForwardDetour),
                       reinterpret_cast<void**>(&g_origForward), "HUD forward accessor")) return false;
    Log::Line("HUD projection: tracking applied at marker and interaction projection reads");
    return true;
}
}  // namespace ThiefHeadTracking
