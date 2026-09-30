#pragma once
// Arizona Games Launcher (Electron app): where it lives and where its SA-MP games are.
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/paths.h"

namespace uf {

struct ArizonaGame {
    std::string id;        // "arizona", "rodina", "village", "*_staging"
    std::string title;     // "Arizona RP"
    fs::path dir;          // settings[id].gamePath or <launcher>\bin\<id>
    bool customPath = false;
    bool autoClean = false;
};

struct ArizonaInfo {
    fs::path launcherDir;
    fs::path settingsFile;
    std::vector<ArizonaGame> games;
};

// SA-MP based game ids of the launcher (arizonav = GTA V and trilogy are skipped on purpose).
inline constexpr const char* kArizonaGameIds[] = {"arizona", "rodina", "village",
                                                   "arizona_staging", "rodina_staging", "village_staging"};

std::optional<fs::path> FindArizonaLauncherDir();
// Pure parser of %APPDATA%\arizona-launcher\settings.json (electron-settings format).
std::vector<ArizonaGame> ParseArizonaSettings(std::string_view json, const fs::path& launcherDir);
ArizonaInfo LoadArizonaInfo(const std::optional<fs::path>& launcherOverride = std::nullopt,
                            const std::optional<fs::path>& settingsOverride = std::nullopt);

std::string ArizonaTitle(std::string_view id);
// <launcher>\bin\<id> layout check (works without settings.json, e.g. in the elevated instance).
std::optional<std::string> ArizonaIdFromLayout(const fs::path& gameDir);
// Files the launcher downloads and verifies itself; the installer never writes them.
bool IsArizonaManagedFile(const fs::path& relPath);

}  // namespace uf
