#pragma once
#include <format>
#include <string>
#include <string_view>
#include <vector>

#include "core/paths.h"

namespace uf::log {

enum class Level { Info, Warn, Error };

// Opens (appends to) the log file. Safe to call once per process; logging before Init only fills the memory buffer.
void Init(const fs::path& file);
fs::path File();
void Write(Level level, std::string_view message);
// Mirrors every line to stdout (used by the CLI).
void EchoToStdout(bool enable);
std::vector<std::string> Recent(std::size_t maxLines = 200);

template <typename... Args>
void Info(std::format_string<Args...> fmt, Args&&... args) {
    Write(Level::Info, std::format(fmt, std::forward<Args>(args)...));
}
template <typename... Args>
void Warn(std::format_string<Args...> fmt, Args&&... args) {
    Write(Level::Warn, std::format(fmt, std::forward<Args>(args)...));
}
template <typename... Args>
void Error(std::format_string<Args...> fmt, Args&&... args) {
    Write(Level::Error, std::format(fmt, std::forward<Args>(args)...));
}

}  // namespace uf::log
