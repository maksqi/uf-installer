#pragma once
#include <string>

#include "core/paths.h"

namespace uf {

// System fonts the script uses via renderCreateFont; not bundled, only reported when missing.
struct WarnFont {
    const char* file;
    const char* name;
};
inline constexpr WarnFont kWarnFonts[] = {{"arial.ttf", "Arial"}, {"tahoma.ttf", "Tahoma"}, {"segoeui.ttf", "Segoe UI"}};

fs::path SystemFontsDir();

// Copies the font into `fontsDir`; with `registerSystem` also adds the HKLM Fonts value and loads it
// into the current session (AddFontResource). Throws FsError on failure.
void InstallFontFile(const fs::path& staged, const fs::path& fontsDir, const std::string& file,
                     const std::string& regName, bool registerSystem);
// Tells running applications that the font list changed.
void BroadcastFontChange();

}  // namespace uf
