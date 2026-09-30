#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/paths.h"

namespace uf {

// Error from a Win32 file operation; what() is UTF-8 and already human readable.
class FsError : public std::runtime_error {
public:
    FsError(unsigned long code, const std::string& message) : std::runtime_error(message), code_(code) {}
    unsigned long code() const { return code_; }
    bool accessDenied() const;

private:
    unsigned long code_;
};

std::string Win32ErrorText(unsigned long code);

bool FileExists(const fs::path& p);
bool DirExists(const fs::path& p);
std::optional<std::uint64_t> FileSize(const fs::path& p);
std::optional<std::uint32_t> FileCrc32(const fs::path& p);
bool FileMatches(const fs::path& p, std::uint64_t size, std::uint32_t crc);

std::optional<std::vector<std::uint8_t>> ReadFileBytes(const fs::path& p, std::size_t maxBytes = SIZE_MAX);
std::optional<std::vector<std::uint8_t>> ReadFileRange(const fs::path& p, std::uint64_t offset, std::size_t n);

// Probes whether files can be created in `dir` (or its nearest existing ancestor).
bool IsDirWritable(const fs::path& dir);
// Probes whether an existing file could be replaced/moved away.
bool IsFileReplaceable(const fs::path& file);

void ClearReadOnly(const fs::path& p);
// Both throw FsError. Transient sharing violations (antivirus, indexer) are retried.
void MoveFileRetry(const fs::path& from, const fs::path& to, bool replaceExisting);
void CopyFileRetry(const fs::path& from, const fs::path& to);
// Creates `dir` and missing parents; returns the created folders, outermost first.
std::vector<fs::path> CreateDirs(const fs::path& dir);
std::optional<std::uint64_t> FreeSpace(const fs::path& dir);

// Adds the \\?\ prefix for paths that would exceed MAX_PATH.
std::wstring ExtendedPath(const fs::path& p);

}  // namespace uf
