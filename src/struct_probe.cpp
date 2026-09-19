// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "struct_probe.h"

#include "logging.h"
#include "memory_probe.h"

#include <windows.h>

#include <cstring>

namespace ThiefHeadTracking {

namespace {

constexpr int kMaxLabels = 8;
constexpr std::uint32_t kMaxBytes = 0x600;

bool g_enabled = false;
const char* g_seen[kMaxLabels] = {};
int g_seenCount = 0;

bool AlreadyDumped(const char* label) {
    for (int i = 0; i < g_seenCount; ++i) {
        if (std::strcmp(g_seen[i], label) == 0) {
            return true;
        }
    }
    return g_seenCount == kMaxLabels;
}

// Recorded only once a dump has actually been written. Claiming the label on entry meant a
// single early frame where the object was not yet readable spent the label for the session,
// so the diagnostic run that exists to produce the missing offsets produced one rejection
// line and never tried again.
void RecordDumped(const char* label) {
    if (g_seenCount < kMaxLabels) {
        g_seen[g_seenCount++] = label;
    }
}

}  // namespace

void SetStructProbeEnabled(bool enabled) {
    g_enabled = enabled;
}

bool StructProbeEnabled() {
    return g_enabled;
}

void ProbeStruct(const char* label, const void* obj, std::uint32_t bytes) {
    if (!g_enabled || obj == nullptr || AlreadyDumped(label)) {
        return;
    }
    if (bytes > kMaxBytes) {
        bytes = kMaxBytes;
    }
    // The object is live engine memory, so the read itself is the risk. The query here is
    // only to find the region end and cut the dump back to it; committed is NOT the same as
    // readable, so the span that survives that trim then goes through the shared test,
    // which is the one that rejects a guard page and a PAGE_NOACCESS page. Both report
    // MEM_COMMIT and both fault, and this runs on the render path from inside a detour -
    // in the diagnostic session whose whole purpose is to produce the missing offsets.
    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(obj, &mbi, sizeof(mbi)) != sizeof(mbi) || mbi.State != MEM_COMMIT) {
        Log::Line("PROBE %s: %p is not committed memory", label, obj);
        return;
    }
    const auto start = reinterpret_cast<std::uintptr_t>(obj);
    const auto regionEnd = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    if (start + bytes > regionEnd) {
        bytes = static_cast<std::uint32_t>(regionEnd - start);
    }
    if (!ReadableSpan(start, bytes)) {
        Log::Line("PROBE %s: %p is committed but not readable", label, obj);
        return;
    }

    RecordDumped(label);
    Log::Line("PROBE %s at %p, %u bytes", label, obj, bytes);
    const auto* base = static_cast<const std::uint8_t*>(obj);
    for (std::uint32_t off = 0; off + 8 <= bytes; off += 8) {
        std::uint64_t q;
        float f[2];
        std::memcpy(&q, base + off, sizeof(q));
        std::memcpy(f, base + off, sizeof(f));
        std::int32_t i[2];
        std::memcpy(i, base + off, sizeof(i));
        Log::Line("PROBE %s +0x%03X  %016llX  f=%-14.5g %-14.5g  i=%-11d %-11d", label, off,
                  static_cast<unsigned long long>(q), f[0], f[1], i[0], i[1]);
    }
}

}  // namespace ThiefHeadTracking
