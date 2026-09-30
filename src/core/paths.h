#pragma once
#include <filesystem>
#include <string>
#include <string_view>

namespace uf {

namespace fs = std::filesystem;

std::string ToUtf8(std::wstring_view s);
std::wstring ToWide(std::string_view s);
inline std::string PathUtf8(const fs::path& p) { return ToUtf8(p.native()); }

std::wstring ToLower(std::wstring_view s);
bool IEquals(std::wstring_view a, std::wstring_view b);
bool IStartsWith(std::wstring_view s, std::wstring_view prefix);
bool IEndsWith(std::wstring_view s, std::wstring_view suffix);
bool IsAscii(std::wstring_view s);

// Absolute, normalized path with symlinks/junctions/8.3 names resolved when the path exists.
fs::path CanonicalPath(const fs::path& p);
// Case-insensitive key of CanonicalPath(), used to deduplicate folders.
std::wstring PathKey(const fs::path& p);
// True when `child` is `parent` or lies inside it (case-insensitive, lexical).
bool IsSubPath(const fs::path& child, const fs::path& parent);

enum class Folder {
    LocalAppData,
    RoamingAppData,
    Fonts,
    Desktop,
    PublicDesktop,
    StartMenu,
    CommonStartMenu,
    Windows,
    Temp,
};
fs::path GetFolder(Folder f);
std::wstring GetEnv(const wchar_t* name);
fs::path ExePath();

// "Program Files" folders of both views (32-bit process sees only the x86 one via FOLDERID).
bool IsUnderProgramFiles(const fs::path& p);

}  // namespace uf
