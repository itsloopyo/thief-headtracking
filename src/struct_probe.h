// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include <cstdint>

namespace ThiefHeadTracking {

// Writes one dump of @p bytes from @p obj to the log, as offset / qword / two floats per
// line, the first time it is called with a given @p label. Nothing is dereferenced: the
// bytes are read flat, which is what makes it safe to point at a live engine object.
//
// This is how the ACamera field offsets in the build profile were established, and it is
// what a session pins them again with after a patch moves them: a field is identified by
// the shape of its value - a field of view sits at 60-90, a rotator component in
// [-32768, 32767], a pointer is an absurd float - and confirmed by watching it change in
// game. It runs only when the INI turns it on, and only once per label per session.
void ProbeStruct(const char* label, const void* obj, std::uint32_t bytes);

// Turns the probe on. Off unless the INI asks for it: the dump is long, and a log a
// player is asked to send should not open with 512 lines of hex.
void SetStructProbeEnabled(bool enabled);
bool StructProbeEnabled();

}  // namespace ThiefHeadTracking
