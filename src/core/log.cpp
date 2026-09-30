#include "core/log.h"

#include <windows.h>

#include <cstdio>
#include <deque>
#include <mutex>

namespace uf::log {

namespace {

struct State {
    std::mutex mutex;
    HANDLE file = INVALID_HANDLE_VALUE;
    fs::path path;
    std::deque<std::string> recent;
    bool echo = false;
};

State& S() {
    static State s;
    return s;
}

}  // namespace

void Init(const fs::path& file) {
    State& s = S();
    std::lock_guard lock(s.mutex);
    if (s.file != INVALID_HANDLE_VALUE) return;
    s.file = CreateFileW(file.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                         OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (s.file == INVALID_HANDLE_VALUE) return;
    s.path = file;
    // Keep the log small: start over when it grows beyond 1 MiB.
    LARGE_INTEGER size{};
    if (GetFileSizeEx(s.file, &size) && size.QuadPart > (1 << 20)) {
        CloseHandle(s.file);
        s.file = CreateFileW(file.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                             nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        size.QuadPart = 0;
    }
    if (size.QuadPart == 0) {  // UTF-8 BOM so that old Notepad shows Cyrillic correctly
        DWORD written = 0;
        WriteFile(s.file, "\xEF\xBB\xBF", 3, &written, nullptr);
    }
    for (const std::string& line : s.recent) {
        DWORD written = 0;
        std::string l = line + "\r\n";
        WriteFile(s.file, l.data(), static_cast<DWORD>(l.size()), &written, nullptr);
    }
}

fs::path File() {
    std::lock_guard lock(S().mutex);
    return S().path;
}

void EchoToStdout(bool enable) {
    std::lock_guard lock(S().mutex);
    S().echo = enable;
}

void Write(Level level, std::string_view message) {
    SYSTEMTIME t;
    GetLocalTime(&t);
    const char* tag = level == Level::Info ? "INFO " : level == Level::Warn ? "WARN " : "ERROR";
    std::string line = std::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:03} [{}] {}", t.wYear, t.wMonth, t.wDay, t.wHour,
                                   t.wMinute, t.wSecond, t.wMilliseconds, tag, message);
    State& s = S();
    std::lock_guard lock(s.mutex);
    if (s.file != INVALID_HANDLE_VALUE) {
        std::string l = line + "\r\n";
        DWORD written = 0;
        WriteFile(s.file, l.data(), static_cast<DWORD>(l.size()), &written, nullptr);
    }
    if (s.echo) {
        std::fwrite(line.data(), 1, line.size(), stdout);
        std::fputc('\n', stdout);
        std::fflush(stdout);
    }
    s.recent.push_back(std::move(line));
    if (s.recent.size() > 2000) s.recent.pop_front();
}

std::vector<std::string> Recent(std::size_t maxLines) {
    std::lock_guard lock(S().mutex);
    const auto& r = S().recent;
    std::size_t start = r.size() > maxLines ? r.size() - maxLines : 0;
    return {r.begin() + static_cast<std::ptrdiff_t>(start), r.end()};
}

}  // namespace uf::log
