#pragma once
// Command line of the installer (mostly test and debug switches).
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/paths.h"

namespace uf {

struct Args {
    std::optional<fs::path> target;         // --target <dir>: open this folder directly
    bool install = false;                   // --install: start installing right away (with --target)
    bool elevated = false;                  // --elevated: we were relaunched via UAC
    bool noElevate = false;                 // --no-elevate: never ask for admin rights
    std::string opts;                       // --opts <encoded Options>
    std::optional<std::uint64_t> planHash;  // --plan-hash <hex>
    std::optional<fs::path> fontsDir;       // --fonts-dir <dir>: install fonts there (tests)
    bool simulateMissingFonts = false;      // --simulate-missing-fonts
    bool simulateMissingD3dx9 = false;      // --simulate-missing-d3dx9
    std::optional<fs::path> arizonaRoot;    // --arizona-root <launcher dir>
    std::optional<fs::path> arizonaSettings;// --arizona-settings <settings.json>
    bool noScan = false;                    // --no-scan: skip the drive scan
    std::optional<fs::path> screenshot;     // --screenshot <png>
    int screenshotDelayMs = 1500;           // --screenshot-delay <ms>
    float dpiScale = 0.f;                   // --dpi-scale <f>
    int throttleMs = 0;                     // --throttle-ms <n>
    int failAfter = -1;                     // --fail-after <n>
    std::optional<std::pair<int, int>> pos; // --pos x,y
    std::string lang;                       // --lang ru|en (default: from Windows)
    std::string theme;                      // --theme dark|light (default: from Windows)
    std::vector<std::wstring> unknown;
};

Args ParseArgs(int argc, wchar_t** argv);
// Parameters for the elevated copy of the GUI: keeps test/debug switches, adds target/options.
std::wstring BuildElevatedParameters(const Args& base, const fs::path& target, const std::string& opts, std::uint64_t planHash,
                                     bool install, std::optional<std::pair<int, int>> pos);

}  // namespace uf
