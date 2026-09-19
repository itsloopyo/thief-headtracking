// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include <windows.h>

#include <cstddef>
#include <cstdint>

namespace ThiefHeadTracking {

// Whether @p bytes starting at @p p can be read without faulting.
//
// Every engine memory read in the mod goes through this or through Readable below. The
// reads run from inside detours on the render path while a level load is rebuilding the
// objects behind those pointers, so an address that is no longer backed has to be
// rejected rather than followed.
//
// Committed is NOT the same as readable, and the difference is what faults: a guard page
// raises on first touch and a PAGE_NOACCESS page raises on every touch, and both report
// MEM_COMMIT. Heaps and thread stacks are full of them, so a stale pointer landing on one
// takes the game down from inside a detour. The protection is checked as well as the
// state.
//
// Shared rather than copied per translation unit: the gate walk, the camera lookup and
// the world trace ask the identical question, and copies of it are things to keep in step.
inline bool ReadableSpan(std::uintptr_t p, std::size_t bytes) {
    if (p == 0) {
        return false;
    }
    MEMORY_BASIC_INFORMATION mbi{};
    const void* addr = reinterpret_cast<const void*>(p);
    if (VirtualQuery(addr, &mbi, sizeof(mbi)) != sizeof(mbi) || mbi.State != MEM_COMMIT) {
        return false;
    }
    constexpr DWORD kReadableProtections =
        PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ |
        PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    // PAGE_GUARD is a modifier ORed onto a base protection, so it survives the mask above
    // and has to be rejected on its own.
    if ((mbi.Protect & kReadableProtections) == 0 || (mbi.Protect & PAGE_GUARD) != 0) {
        return false;
    }
    const auto regionEnd = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    return p + bytes <= regionEnd;
}

// As ReadableSpan, plus the test that says this address is an OBJECT POINTER rather than
// a torn read: every UE3 object on this build is 8-aligned, so an unaligned value is a
// pointer caught half-written.
//
// The two are separate because that alignment rule belongs to pointers alone. A struct
// FIELD sits at whatever offset the compiler gave it - a DWORD of bitflags is routinely
// 4-aligned - so validating a field address with the pointer test would reject a perfectly
// good read and silently switch off whatever gate depended on it. Field reads use
// ReadableSpan; pointer hops use this.
inline bool Readable(std::uintptr_t p, std::size_t bytes) {
    return (p & 7u) == 0 && ReadableSpan(p, bytes);
}

}  // namespace ThiefHeadTracking
