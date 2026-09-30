#include "core/paths.h"

#include <windows.h>
#include <knownfolders.h>
#include <shlobj.h>

namespace uf {

std::string ToUtf8(std::wstring_view s) {
    if (s.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n, nullptr, nullptr);
    return out;
}

std::wstring ToWide(std::string_view s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n);
    return out;
}

std::wstring ToLower(std::wstring_view s) {
    std::wstring r(s);
    if (!r.empty()) CharLowerBuffW(r.data(), static_cast<DWORD>(r.size()));
    return r;
}

bool IEquals(std::wstring_view a, std::wstring_view b) {
    if (a.size() != b.size()) return false;
    if (a.empty()) return true;
    return CompareStringOrdinal(a.data(), static_cast<int>(a.size()), b.data(), static_cast<int>(b.size()), TRUE) == CSTR_EQUAL;
}

bool IStartsWith(std::wstring_view s, std::wstring_view prefix) {
    return s.size() >= prefix.size() && IEquals(s.substr(0, prefix.size()), prefix);
}

bool IEndsWith(std::wstring_view s, std::wstring_view suffix) {
    return s.size() >= suffix.size() && IEquals(s.substr(s.size() - suffix.size()), suffix);
}

bool IsAscii(std::wstring_view s) {
    for (wchar_t c : s)
        if (c >= 128) return false;
    return true;
}

static fs::path StripTrailingSeparator(fs::path p) {
    if (!p.has_filename() && p.has_relative_path()) p = p.parent_path();
    return p;
}

fs::path CanonicalPath(const fs::path& p) {
    std::error_code ec;
    fs::path abs = fs::absolute(p, ec);
    if (ec) abs = p;
    abs = StripTrailingSeparator(abs.lexically_normal());

    HANDLE h = CreateFileW(abs.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                           OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (h == INVALID_HANDLE_VALUE) return abs;
    std::wstring buf(1024, L'\0');
    DWORD n = GetFinalPathNameByHandleW(h, buf.data(), static_cast<DWORD>(buf.size()), FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (n >= buf.size()) {
        buf.resize(n + 1);
        n = GetFinalPathNameByHandleW(h, buf.data(), static_cast<DWORD>(buf.size()), FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    }
    CloseHandle(h);
    if (n == 0 || n >= buf.size()) return abs;
    buf.resize(n);
    if (buf.starts_with(L"\\\\?\\UNC\\"))
        buf = L"\\\\" + buf.substr(8);
    else if (buf.starts_with(L"\\\\?\\"))
        buf = buf.substr(4);
    return StripTrailingSeparator(fs::path(buf));
}

std::wstring PathKey(const fs::path& p) { return ToLower(CanonicalPath(p).native()); }

bool IsSubPath(const fs::path& child, const fs::path& parent) {
    std::wstring c = ToLower(StripTrailingSeparator(child.lexically_normal()).native());
    std::wstring p = ToLower(StripTrailingSeparator(parent.lexically_normal()).native());
    if (p.empty()) return false;
    if (c.size() < p.size() || c.compare(0, p.size(), p) != 0) return false;
    return c.size() == p.size() || c[p.size()] == L'\\' || p.back() == L'\\';
}

static fs::path KnownFolder(const KNOWNFOLDERID& id) {
    PWSTR raw = nullptr;
    fs::path result;
    if (SUCCEEDED(SHGetKnownFolderPath(id, KF_FLAG_DONT_VERIFY, nullptr, &raw)) && raw) result = raw;
    CoTaskMemFree(raw);
    return result;
}

std::wstring GetEnv(const wchar_t* name) {
    DWORD n = GetEnvironmentVariableW(name, nullptr, 0);
    if (n == 0) return {};
    std::wstring v(n, L'\0');
    n = GetEnvironmentVariableW(name, v.data(), n);
    v.resize(n);
    return v;
}

fs::path GetFolder(Folder f) {
    switch (f) {
        case Folder::LocalAppData: return KnownFolder(FOLDERID_LocalAppData);
        case Folder::RoamingAppData: return KnownFolder(FOLDERID_RoamingAppData);
        case Folder::Fonts: {
            fs::path p = KnownFolder(FOLDERID_Fonts);
            return p.empty() ? GetFolder(Folder::Windows) / L"Fonts" : p;
        }
        case Folder::Desktop: return KnownFolder(FOLDERID_Desktop);
        case Folder::PublicDesktop: return KnownFolder(FOLDERID_PublicDesktop);
        case Folder::StartMenu: return KnownFolder(FOLDERID_StartMenu);
        case Folder::CommonStartMenu: return KnownFolder(FOLDERID_CommonStartMenu);
        case Folder::Windows: {
            wchar_t buf[MAX_PATH];
            UINT n = GetWindowsDirectoryW(buf, MAX_PATH);
            return n ? fs::path(std::wstring(buf, n)) : fs::path(L"C:\\Windows");
        }
        case Folder::Temp: {
            wchar_t buf[MAX_PATH + 1];
            DWORD n = GetTempPathW(MAX_PATH + 1, buf);
            return n ? StripTrailingSeparator(fs::path(std::wstring(buf, n))) : fs::path(L"C:\\Windows\\Temp");
        }
    }
    return {};
}

fs::path ExePath() {
    std::wstring buf(MAX_PATH, L'\0');
    for (;;) {
        DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
        if (n < buf.size()) {
            buf.resize(n);
            return buf;
        }
        buf.resize(buf.size() * 2);
    }
}

bool IsUnderProgramFiles(const fs::path& p) {
    for (const wchar_t* var : {L"ProgramW6432", L"ProgramFiles", L"ProgramFiles(x86)"}) {
        std::wstring v = GetEnv(var);
        if (!v.empty() && IsSubPath(p, v)) return true;
    }
    return false;
}

}  // namespace uf
