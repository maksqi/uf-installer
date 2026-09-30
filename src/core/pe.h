#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/paths.h"

namespace uf::pe {

struct Section {
    std::uint32_t va = 0;
    std::uint32_t virtualSize = 0;
    std::uint32_t rawOffset = 0;
    std::uint32_t rawSize = 0;
};

struct Info {
    std::uint16_t machine = 0;  // 0x14c = i386
    std::uint32_t entryRva = 0;
    std::uint32_t imageBase = 0;
    std::vector<Section> sections;
};

std::optional<Info> Parse(std::span<const std::uint8_t> headerBytes);
std::optional<Info> ParseFile(const fs::path& p);
std::optional<std::uint32_t> VaToFileOffset(const Info& info, std::uint32_t va);
bool IsI386(const fs::path& p);

// "a.b.c.d" from VS_FIXEDFILEINFO, empty if there is no version resource.
std::string FileVersion(const fs::path& p);

// Finds `prefix` (ASCII) and returns it together with the printable characters that follow.
std::string FindAsciiString(std::span<const std::uint8_t> data, std::string_view prefix, std::size_t maxLen = 96);
// Case-insensitive search for an ASCII needle stored as ASCII or UTF-16LE.
bool ContainsText(std::span<const std::uint8_t> data, std::string_view needle);

}  // namespace uf::pe
