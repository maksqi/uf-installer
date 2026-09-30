#pragma once
// Turns a FolderReport + user options into a list of install actions. Pure function, no I/O.
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/inspect.h"

namespace uf {

enum class ItemId { Gta, Samp, AsiLoader, Cleo, Sampfuncs, MoonLoader, Libs, Script, Fonts, DirectX, Count };
inline constexpr std::size_t kItemCount = static_cast<std::size_t>(ItemId::Count);

enum class ItemState {
    Ok,       // present, nothing to do
    Install,  // missing, will be installed
    Update,   // present, will be replaced
    Repair,   // partially present, missing parts will be added
    Skipped,  // user unchecked the action
    Managed,  // owned by Arizona Launcher, not touched
    Warning,  // present but with a problem (info only)
    Error,    // missing and cannot be installed by us
};

struct PlanItem {
    ItemId id{};
    ItemState state = ItemState::Ok;
    std::string title;
    std::string status;      // short: "CLEO 4.3.22", "будет установлен"
    std::string detail;      // longer explanation / file list
    bool toggleable = false; // a checkbox is shown
    bool enabled = false;    // checkbox value (action will run)
    bool admin = false;      // action needs administrator rights
};

enum class Severity { Info, Warning, Error };
struct Notice {
    Severity severity = Severity::Info;
    std::string text;
};

enum class OpKind { Copy, MoveToBackup };
enum class CopyMode { AddIfMissing, Replace };

struct FileOp {
    OpKind kind = OpKind::Copy;
    ItemId item{};
    std::string entry;  // payload.zip entry (Copy)
    fs::path rel;       // relative to the game folder
    CopyMode mode = CopyMode::AddIfMissing;
    std::uint32_t size = 0;
    std::uint32_t crc = 0;
};

struct FontOp {
    std::string entry;
    std::string file;
    std::string regName;
    std::uint32_t size = 0;
    std::uint32_t crc = 0;
};

struct Options {
    std::array<std::optional<bool>, kItemCount> toggles{};  // user overrides of PlanItem::enabled
    bool overwriteLibs = false;
    bool customFontsDir = false;  // fonts go to a test folder: no admin needed

    void Set(ItemId id, bool v) { toggles[static_cast<std::size_t>(id)] = v; }
};

struct InstallPlan {
    std::vector<PlanItem> items;
    std::vector<Notice> notices;
    std::vector<FileOp> files;
    std::vector<fs::path> dirs;
    std::vector<FontOp> fonts;
    bool directx = false;

    bool needsAdmin = false;
    std::vector<std::string> adminReasons;
    bool blocked = false;
    std::string blockReason;

    bool Empty() const { return files.empty() && dirs.empty() && fonts.empty() && !directx; }
    std::uint64_t Hash() const;
    std::uint64_t BytesToWrite() const;
    const PlanItem* Find(ItemId id) const;
};

InstallPlan BuildPlan(const FolderReport& report, const Options& options);

std::string EncodeOptions(const Options& o);
Options DecodeOptions(std::string_view s);

}  // namespace uf
