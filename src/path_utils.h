// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include <string>

namespace ThiefHeadTracking {

// A file beside this module, as a full wide path. Read with the WIDE API rather than
// converted from the ANSI one: GetModuleFileNameA renders anything the active ANSI codepage
// cannot represent as '?', which turns a non-ASCII install path into a directory that does
// not exist.
//
// Returns an empty string for a module path GetModuleFileName truncated into its buffer, and
// for a directory that fits MAX_PATH but no longer does once the filename is on the end of it.
std::wstring GetModulePathW(const char* filename);

// The folder this module was loaded from, with its trailing separator, read the same way.
// Empty when GetModuleFileName truncated the module path.
std::wstring GetModuleDirectoryW();

// The ANSI path the pre-canonical builds opened @p path by, for the legacy import: the path
// itself where the active ANSI codepage holds every character of its folder, else with the
// folder's 8.3 short name. Empty where neither gives a path or the result does not fit
// MAX_PATH; those builds did not start there.
std::string LegacyAnsiPath(const std::wstring& path);

}  // namespace ThiefHeadTracking
