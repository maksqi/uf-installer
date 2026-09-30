#pragma once
// Executes an InstallPlan: stage into %TEMP% -> commit with a journal -> verify, rolling back on failure.
#include <functional>
#include <stop_token>
#include <string>
#include <vector>

#include "core/plan.h"

namespace uf {

struct InstallEnv {
    fs::path fontsDir;            // target folder for fonts (%WINDIR%\Fonts normally)
    bool registerFonts = true;    // write HKLM Fonts values + AddFontResource
    int failAfter = -1;           // test hook: throw after N committed file operations
    int throttleMs = 0;           // test hook: slow down every file operation
};

struct InstallProgress {
    float fraction = 0.f;
    std::string stage;
    std::string current;
};

struct InstallResult {
    bool ok = false;
    bool rolledBack = false;
    std::string error;
    fs::path backupDir;  // empty when nothing was backed up
    int installed = 0;
    int replaced = 0;
    int skipped = 0;
    int movedOld = 0;
    int dirsCreated = 0;
    int fontsInstalled = 0;
    bool directxInstalled = false;
    std::vector<std::string> warnings;
    double seconds = 0;
};

using ProgressFn = std::function<void(const InstallProgress&)>;
using LogFn = std::function<void(const std::string&)>;

InstallResult RunInstall(const fs::path& gameDir, const InstallPlan& plan, const InstallEnv& env,
                         const ProgressFn& progress, const LogFn& logLine, std::stop_token stop = {});

// Folder for temporary files; removed (recursively) on destruction.
class TempDir {
public:
    TempDir();
    ~TempDir();
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    const fs::path& path() const { return path_; }

private:
    fs::path path_;
};

// Deletes %TEMP%\uf-installer-* folders left by crashed runs (older than a day).
void CleanupStaleTempDirs();

}  // namespace uf
