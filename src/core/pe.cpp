#include "core/pe.h"

#include <windows.h>

#include <algorithm>
#include <cstring>
#include <format>

#include "core/fsutil.h"

namespace uf::pe {

namespace {

template <typename T>
bool Read(std::span<const std::uint8_t> b, std::size_t off, T& out) {
    if (off + sizeof(T) > b.size()) return false;
    std::memcpy(&out, b.data() + off, sizeof(T));
    return true;
}

char LowerAscii(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c; }

}  // namespace

std::optional<Info> Parse(std::span<const std::uint8_t> b) {
    if (b.size() < 0x40 || b[0] != 'M' || b[1] != 'Z') return std::nullopt;
    std::uint32_t off = 0;
    if (!Read(b, 0x3C, off) || off + 24 > b.size()) return std::nullopt;
    if (std::memcmp(b.data() + off, "PE\0\0", 4) != 0) return std::nullopt;

    Info info;
    std::uint16_t sectionCount = 0, optSize = 0, magic = 0;
    Read(b, off + 4, info.machine);
    Read(b, off + 6, sectionCount);
    Read(b, off + 20, optSize);
    std::size_t opt = off + 24;
    if (!Read(b, opt, magic) || !Read(b, opt + 16, info.entryRva)) return std::nullopt;
    if (magic == 0x10B) {
        Read(b, opt + 28, info.imageBase);
    } else {
        std::uint64_t base64 = 0;
        Read(b, opt + 24, base64);
        info.imageBase = static_cast<std::uint32_t>(base64);
    }
    std::size_t sec = opt + optSize;
    for (std::uint16_t i = 0; i < sectionCount; ++i) {
        std::size_t s = sec + static_cast<std::size_t>(i) * 40;
        Section section;
        if (!Read(b, s + 8, section.virtualSize) || !Read(b, s + 12, section.va) || !Read(b, s + 16, section.rawSize) ||
            !Read(b, s + 20, section.rawOffset))
            break;
        info.sections.push_back(section);
    }
    return info;
}

std::optional<Info> ParseFile(const fs::path& p) {
    auto bytes = ReadFileBytes(p, 8192);
    if (!bytes) return std::nullopt;
    return Parse(*bytes);
}

std::optional<std::uint32_t> VaToFileOffset(const Info& info, std::uint32_t va) {
    if (va < info.imageBase) return std::nullopt;
    std::uint32_t rva = va - info.imageBase;
    for (const Section& s : info.sections) {
        std::uint32_t span = std::max(s.virtualSize, s.rawSize);
        if (rva >= s.va && rva < s.va + span) {
            std::uint32_t delta = rva - s.va;
            if (delta >= s.rawSize) return std::nullopt;  // lives in uninitialized data
            return s.rawOffset + delta;
        }
    }
    return std::nullopt;
}

bool IsI386(const fs::path& p) {
    auto info = ParseFile(p);
    return info && info->machine == 0x14C;
}

std::string FileVersion(const fs::path& p) {
    DWORD dummy = 0;
    DWORD size = GetFileVersionInfoSizeW(p.c_str(), &dummy);
    if (size == 0) return {};
    std::vector<std::uint8_t> data(size);
    if (!GetFileVersionInfoW(p.c_str(), 0, size, data.data())) return {};
    VS_FIXEDFILEINFO* fixed = nullptr;
    UINT len = 0;
    if (!VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&fixed), &len) || !fixed || len < sizeof(*fixed))
        return {};
    return std::format("{}.{}.{}.{}", HIWORD(fixed->dwFileVersionMS), LOWORD(fixed->dwFileVersionMS),
                       HIWORD(fixed->dwFileVersionLS), LOWORD(fixed->dwFileVersionLS));
}

std::string FindAsciiString(std::span<const std::uint8_t> data, std::string_view prefix, std::size_t maxLen) {
    auto it = std::search(data.begin(), data.end(), prefix.begin(), prefix.end(),
                          [](std::uint8_t a, char b) { return a == static_cast<std::uint8_t>(b); });
    if (it == data.end()) return {};
    std::string out;
    for (auto p = it; p != data.end() && out.size() < maxLen; ++p) {
        std::uint8_t c = *p;
        if (c < 0x20 || c > 0x7E) break;
        out.push_back(static_cast<char>(c));
    }
    return out;
}

bool ContainsText(std::span<const std::uint8_t> data, std::string_view needle) {
    if (needle.empty()) return true;
    std::string lowered(needle);
    for (char& c : lowered) c = LowerAscii(c);
    auto ascii = std::search(data.begin(), data.end(), lowered.begin(), lowered.end(),
                             [](std::uint8_t a, char b) { return LowerAscii(static_cast<char>(a)) == b; });
    if (ascii != data.end()) return true;
    // UTF-16LE: every second byte is zero.
    if (data.size() < lowered.size() * 2) return false;
    for (std::size_t i = 0; i + lowered.size() * 2 <= data.size(); ++i) {
        bool match = true;
        for (std::size_t k = 0; k < lowered.size(); ++k) {
            if (data[i + 2 * k + 1] != 0 || LowerAscii(static_cast<char>(data[i + 2 * k])) != lowered[k]) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

}  // namespace uf::pe
