#include "core/fsutil.h"
#include "core/i18n.h"

#include <windows.h>

#include <miniz.h>

#include <format>

namespace uf {

namespace {

struct Handle {
    HANDLE h = INVALID_HANDLE_VALUE;
    explicit Handle(HANDLE v) : h(v) {}
    ~Handle() {
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    bool ok() const { return h != INVALID_HANDLE_VALUE; }
};

Handle OpenRead(const fs::path& p) {
    return Handle(CreateFileW(ExtendedPath(p).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
}

bool Transient(DWORD e) {
    return e == ERROR_SHARING_VIOLATION || e == ERROR_LOCK_VIOLATION || e == ERROR_ACCESS_DENIED ||
           e == ERROR_USER_MAPPED_FILE;
}

}  // namespace

bool FsError::accessDenied() const {
    return code_ == ERROR_ACCESS_DENIED || code_ == ERROR_PRIVILEGE_NOT_HELD || code_ == ERROR_ELEVATION_REQUIRED;
}

std::string Win32ErrorText(unsigned long code) {
    wchar_t* buf = nullptr;
    DWORD n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                             nullptr, code, 0, reinterpret_cast<LPWSTR>(&buf), 0, nullptr);
    std::wstring msg = n ? std::wstring(buf, n) : L"";
    LocalFree(buf);
    while (!msg.empty() && (msg.back() == L'\n' || msg.back() == L'\r' || msg.back() == L' ' || msg.back() == L'.'))
        msg.pop_back();
    return F("{} (код {})", "{} (code {})", ToUtf8(msg), code);
}

std::wstring ExtendedPath(const fs::path& p) {
    const std::wstring& s = p.native();
    if (s.size() < 240 || s.starts_with(L"\\\\?\\") || !p.is_absolute()) return s;
    if (s.starts_with(L"\\\\")) return L"\\\\?\\UNC\\" + s.substr(2);
    return L"\\\\?\\" + s;
}

bool FileExists(const fs::path& p) {
    DWORD a = GetFileAttributesW(ExtendedPath(p).c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

bool DirExists(const fs::path& p) {
    DWORD a = GetFileAttributesW(ExtendedPath(p).c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

std::optional<std::uint64_t> FileSize(const fs::path& p) {
    WIN32_FILE_ATTRIBUTE_DATA d{};
    if (!GetFileAttributesExW(ExtendedPath(p).c_str(), GetFileExInfoStandard, &d)) return std::nullopt;
    if (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) return std::nullopt;
    return (static_cast<std::uint64_t>(d.nFileSizeHigh) << 32) | d.nFileSizeLow;
}

std::optional<std::uint32_t> FileCrc32(const fs::path& p) {
    Handle f = OpenRead(p);
    if (!f.ok()) return std::nullopt;
    std::vector<std::uint8_t> buf(1 << 20);
    mz_ulong crc = MZ_CRC32_INIT;
    for (;;) {
        DWORD got = 0;
        if (!ReadFile(f.h, buf.data(), static_cast<DWORD>(buf.size()), &got, nullptr)) return std::nullopt;
        if (got == 0) break;
        crc = mz_crc32(crc, buf.data(), got);
    }
    return static_cast<std::uint32_t>(crc);
}

bool FileMatches(const fs::path& p, std::uint64_t size, std::uint32_t crc) {
    auto s = FileSize(p);
    if (!s || *s != size) return false;
    auto c = FileCrc32(p);
    return c && *c == crc;
}

std::optional<std::vector<std::uint8_t>> ReadFileBytes(const fs::path& p, std::size_t maxBytes) {
    Handle f = OpenRead(p);
    if (!f.ok()) return std::nullopt;
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(f.h, &size)) return std::nullopt;
    std::size_t n = static_cast<std::size_t>(std::min<std::uint64_t>(static_cast<std::uint64_t>(size.QuadPart), maxBytes));
    std::vector<std::uint8_t> data(n);
    std::size_t done = 0;
    while (done < n) {
        DWORD got = 0;
        DWORD want = static_cast<DWORD>(std::min<std::size_t>(n - done, 1u << 24));
        if (!ReadFile(f.h, data.data() + done, want, &got, nullptr) || got == 0) break;
        done += got;
    }
    data.resize(done);
    return data;
}

std::optional<std::vector<std::uint8_t>> ReadFileRange(const fs::path& p, std::uint64_t offset, std::size_t n) {
    Handle f = OpenRead(p);
    if (!f.ok()) return std::nullopt;
    LARGE_INTEGER pos{};
    pos.QuadPart = static_cast<LONGLONG>(offset);
    if (!SetFilePointerEx(f.h, pos, nullptr, FILE_BEGIN)) return std::nullopt;
    std::vector<std::uint8_t> data(n);
    DWORD got = 0;
    if (!ReadFile(f.h, data.data(), static_cast<DWORD>(n), &got, nullptr)) return std::nullopt;
    data.resize(got);
    return data;
}

bool IsDirWritable(const fs::path& dir) {
    fs::path d = dir;
    while (!DirExists(d)) {
        fs::path parent = d.parent_path();
        if (parent.empty() || parent == d) return false;
        d = parent;
    }
    for (int attempt = 0; attempt < 5; ++attempt) {
        fs::path probe = d / std::format(L".uf-probe-{}-{}-{}.tmp", GetCurrentProcessId(), GetTickCount(), attempt);
        HANDLE h = CreateFileW(ExtendedPath(probe).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                               FILE_ATTRIBUTE_TEMPORARY | FILE_ATTRIBUTE_HIDDEN | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            CloseHandle(h);
            return true;
        }
        if (GetLastError() != ERROR_FILE_EXISTS) return false;
    }
    return false;
}

bool IsFileReplaceable(const fs::path& file) {
    HANDLE h = CreateFileW(ExtendedPath(file).c_str(), DELETE | FILE_WRITE_ATTRIBUTES,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                           FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        DWORD e = GetLastError();
        return e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND || e == ERROR_SHARING_VIOLATION;
    }
    CloseHandle(h);
    return true;
}

void ClearReadOnly(const fs::path& p) {
    std::wstring ep = ExtendedPath(p);
    DWORD a = GetFileAttributesW(ep.c_str());
    if (a != INVALID_FILE_ATTRIBUTES && (a & (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)))
        SetFileAttributesW(ep.c_str(), a & ~(FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM));
}

void MoveFileRetry(const fs::path& from, const fs::path& to, bool replaceExisting) {
    DWORD flags = MOVEFILE_WRITE_THROUGH | MOVEFILE_COPY_ALLOWED | (replaceExisting ? MOVEFILE_REPLACE_EXISTING : 0);
    for (int i = 0;; ++i) {
        if (MoveFileExW(ExtendedPath(from).c_str(), ExtendedPath(to).c_str(), flags)) return;
        DWORD e = GetLastError();
        if (i < 5 && Transient(e)) {
            if (e == ERROR_ACCESS_DENIED && replaceExisting) ClearReadOnly(to);
            Sleep(100u << i);
            continue;
        }
        throw FsError(e, F("Не удалось переместить «{}» → «{}»: {}", "Could not move \"{}\" to \"{}\": {}", PathUtf8(from), PathUtf8(to),
                           Win32ErrorText(e)));
    }
}

void CopyFileRetry(const fs::path& from, const fs::path& to) {
    for (int i = 0;; ++i) {
        if (CopyFileW(ExtendedPath(from).c_str(), ExtendedPath(to).c_str(), FALSE)) {
            SetFileAttributesW(ExtendedPath(to).c_str(), FILE_ATTRIBUTE_NORMAL);
            return;
        }
        DWORD e = GetLastError();
        if (i < 5 && Transient(e)) {
            Sleep(100u << i);
            continue;
        }
        throw FsError(e, F("Не удалось записать «{}»: {}", "Could not write \"{}\": {}", PathUtf8(to), Win32ErrorText(e)));
    }
}

std::vector<fs::path> CreateDirs(const fs::path& dir) {
    std::vector<fs::path> missing;
    for (fs::path d = dir; !d.empty() && !DirExists(d); d = d.parent_path()) {
        missing.push_back(d);
        if (d.parent_path() == d) break;
    }
    std::vector<fs::path> created;
    for (auto it = missing.rbegin(); it != missing.rend(); ++it) {
        if (CreateDirectoryW(ExtendedPath(*it).c_str(), nullptr)) {
            created.push_back(*it);
            continue;
        }
        DWORD e = GetLastError();
        if (e == ERROR_ALREADY_EXISTS && DirExists(*it)) continue;
        throw FsError(e, F("Не удалось создать папку «{}»: {}", "Could not create the folder \"{}\": {}", PathUtf8(*it), Win32ErrorText(e)));
    }
    return created;
}

std::optional<std::uint64_t> FreeSpace(const fs::path& dir) {
    fs::path d = dir;
    while (!DirExists(d)) {
        fs::path parent = d.parent_path();
        if (parent.empty() || parent == d) return std::nullopt;
        d = parent;
    }
    ULARGE_INTEGER avail{};
    if (!GetDiskFreeSpaceExW(d.c_str(), &avail, nullptr, nullptr)) return std::nullopt;
    return avail.QuadPart;
}

}  // namespace uf
