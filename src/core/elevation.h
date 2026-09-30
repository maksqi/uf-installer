#pragma once
#include <string>
#include <string_view>

#include "core/paths.h"

namespace uf {

bool IsProcessElevated();

enum class ElevateResult { Started, Cancelled, Failed };
// Starts this exe again via the "runas" verb (UAC prompt). `owner` is an HWND (may be null).
ElevateResult RelaunchElevated(void* owner, const std::wstring& parameters, unsigned long* error = nullptr);

// Quotes one argument following the CommandLineToArgvW rules (handles trailing backslashes).
std::wstring QuoteArg(std::wstring_view arg);
// Mapped network drives are not visible to the elevated process: Z:\games -> \\server\share\games.
fs::path ToUncIfMapped(const fs::path& p);

}  // namespace uf
