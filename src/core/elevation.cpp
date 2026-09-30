#include "core/elevation.h"

#include <windows.h>
#include <shellapi.h>
#include <winnetwk.h>

#include <vector>

namespace uf {

bool IsProcessElevated() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION elevation{};
    DWORD size = 0;
    BOOL ok = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size);
    CloseHandle(token);
    return ok && elevation.TokenIsElevated;
}

ElevateResult RelaunchElevated(void* owner, const std::wstring& parameters, unsigned long* error) {
    std::wstring exe = ExePath().native();
    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    sei.hwnd = static_cast<HWND>(owner);
    sei.lpVerb = L"runas";
    sei.lpFile = exe.c_str();
    sei.lpParameters = parameters.c_str();
    sei.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&sei)) {
        DWORD e = GetLastError();
        if (error) *error = e;
        return e == ERROR_CANCELLED ? ElevateResult::Cancelled : ElevateResult::Failed;
    }
    if (sei.hProcess) {
        AllowSetForegroundWindow(GetProcessId(sei.hProcess));
        CloseHandle(sei.hProcess);
    }
    return ElevateResult::Started;
}

std::wstring QuoteArg(std::wstring_view arg) {
    if (!arg.empty() && arg.find_first_of(L" \t\n\v\"") == std::wstring_view::npos) return std::wstring(arg);
    std::wstring out = L"\"";
    for (std::size_t i = 0;; ++i) {
        std::size_t backslashes = 0;
        while (i < arg.size() && arg[i] == L'\\') {
            ++backslashes;
            ++i;
        }
        if (i == arg.size()) {
            out.append(backslashes * 2, L'\\');
            break;
        }
        if (arg[i] == L'"') {
            out.append(backslashes * 2 + 1, L'\\');
            out.push_back(L'"');
        } else {
            out.append(backslashes, L'\\');
            out.push_back(arg[i]);
        }
    }
    out.push_back(L'"');
    return out;
}

fs::path ToUncIfMapped(const fs::path& p) {
    std::wstring s = p.native();
    if (s.size() < 2 || s[1] != L':') return p;
    std::wstring root = s.substr(0, 2) + L"\\";
    if (GetDriveTypeW(root.c_str()) != DRIVE_REMOTE) return p;
    std::vector<BYTE> buf(4096);
    DWORD size = static_cast<DWORD>(buf.size());
    if (WNetGetUniversalNameW(s.c_str(), UNIVERSAL_NAME_INFO_LEVEL, buf.data(), &size) != NO_ERROR) return p;
    auto* info = reinterpret_cast<UNIVERSAL_NAME_INFOW*>(buf.data());
    return info->lpUniversalName ? fs::path(info->lpUniversalName) : p;
}

}  // namespace uf
