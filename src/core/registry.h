#pragma once
#include <windows.h>

#include <optional>
#include <string>
#include <vector>

namespace uf::reg {

// `view` is 0, KEY_WOW64_32KEY or KEY_WOW64_64KEY.
std::optional<std::wstring> ReadString(HKEY root, const std::wstring& subkey, const wchar_t* value, REGSAM view = 0);
std::vector<std::wstring> ValueNames(HKEY root, const std::wstring& subkey, REGSAM view = 0);
std::vector<std::wstring> SubKeys(HKEY root, const std::wstring& subkey, REGSAM view = 0);
// Returns the Win32 error code (ERROR_SUCCESS on success).
LONG WriteString(HKEY root, const std::wstring& subkey, const wchar_t* value, const std::wstring& data, REGSAM view = 0);

}  // namespace uf::reg
