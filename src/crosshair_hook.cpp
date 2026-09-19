// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "crosshair_hook.h"
#include "aim_marker.h"
#include "logging.h"
#include "memory_probe.h"
#include "reticle_projection.h"

#include <windows.h>
#include <cstring>

namespace ThiefHeadTracking {
namespace {

using GetWidget_t = void*(__fastcall*)(void*, int);
using SetPosition_t = void(__fastcall*)(void*, float, float);
using GetFrameRect_t = void(__fastcall*)(void*, float*, float*, float*, float*);
const BuildProfile* g_profile = nullptr;
std::uintptr_t g_instance = 0;
SetPosition_t g_setPosition = nullptr;
GetFrameRect_t g_getFrameRect = nullptr;

std::uintptr_t ReadPointer(std::uintptr_t field) {
    std::uintptr_t value = 0;
    if (ReadableSpan(field, sizeof(value))) {
        std::memcpy(&value, reinterpret_cast<const void*>(field), sizeof(value));
    }
    return value;
}

void* ReticleWidget() {
    const auto instance = ReadPointer(g_instance);
    if (!instance) return nullptr;
    const auto ui = ReadPointer(instance + g_profile->offGameUI);
    if (!ui) return nullptr;
    const auto vtable = ReadPointer(ui);
    if (!vtable) return nullptr;
    const auto getter = ReadPointer(vtable + g_profile->vtUIGetWidget);
    if (!getter) return nullptr;
    const auto hud = reinterpret_cast<std::uintptr_t>(
        reinterpret_cast<GetWidget_t>(getter)(reinterpret_cast<void*>(ui), 2));
    if (!hud) return nullptr;
    return reinterpret_cast<void*>(ReadPointer(hud + g_profile->offMarksmanWidget));
}

}  // namespace

void InitCrosshair(const BuildProfile& profile, std::uintptr_t moduleBase) {
    g_profile = &profile;
    g_instance = moduleBase + profile.rvaGameInstance;
    g_setPosition = reinterpret_cast<SetPosition_t>(moduleBase + profile.rvaGfxSetPosition);
    g_getFrameRect = reinterpret_cast<GetFrameRect_t>(moduleBase + profile.rvaGfxGetVisibleFrameRect);
    Log::Line("Reticle: projecting the clean aim through the rendered scene projection");
}

void UpdateCrosshair(const void* sceneView, bool trackingInjected) {
    if (!g_profile) return;
    void* widget = ReticleWidget();
    if (!widget) return;
    const auto movie = ReadPointer(reinterpret_cast<std::uintptr_t>(widget) + g_profile->offGfxMovie);
    if (!movie) return;
    ReticleStage stage{};
    g_getFrameRect(reinterpret_cast<void*>(movie), &stage.left, &stage.top,
                   &stage.right, &stage.bottom);
    const float width = stage.right - stage.left;
    const float height = stage.bottom - stage.top;
    if (!std::isfinite(width) || !std::isfinite(height) || width <= 0 || height <= 0) return;

    float x = (stage.left + stage.right) * 0.5f;
    float y = (stage.top + stage.bottom) * 0.5f;
    const auto aim = SampleAimMarker(GetAimMarker());
    bool visible = true;
    float projection[16] = {};
    if (trackingInjected) {
        const auto address = reinterpret_cast<std::uintptr_t>(sceneView) + g_profile->offSceneProjection;
        std::memcpy(projection, reinterpret_cast<const void*>(address), sizeof(projection));
        visible = aim.active && ProjectReticleToStage(stage, projection[0], projection[5],
                                        aim.right, aim.up, aim.forward, x, y);
        // Keep the game's visibility state intact. An off-screen target must not
        // become an on-screen aim point at the edge of the display.
        if (!visible) x = stage.right + width;
    }
    g_setPosition(widget, x, y);

    static ULONGLONG lastReport = 0;
    const auto now = GetTickCount64();
    if (now - lastReport >= 2000) {
        lastReport = now;
        Log::Line("Reticle: active=%d visible=%d stage=(%.1f,%.1f,%.1f,%.1f) "
                  "projection=(%.4f,%.4f) aim=(%.2f,%.2f,%.2f) xy=(%.2f,%.2f) distance=%.1f",
                  aim.active, visible, stage.left, stage.top, stage.right, stage.bottom,
                  projection[0], projection[5], aim.right, aim.up, aim.forward, x, y, aim.distance);
    }
}

}  // namespace ThiefHeadTracking
