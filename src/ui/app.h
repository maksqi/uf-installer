#pragma once
#include <windows.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "core/args.h"
#include "core/discovery.h"
#include "core/installer.h"
#include "core/plan.h"

namespace uf::ui {

inline constexpr UINT WM_APP_WAKE = WM_APP + 1;
inline constexpr float kTitleBarHeight = 44.f;  // logical px
inline constexpr float kCaptionButtonWidth = 46.f;  // minimize / close
inline constexpr float kWindowWidth = 900.f;
inline constexpr float kWindowHeight = 636.f;
// Everything is drawn 10% larger than the system DPI alone would give: easier to read.
inline constexpr float kUiZoom = 1.1f;

enum class Screen { Select, Analysis, Progress, Done };

struct Banner {
    Severity severity = Severity::Info;
    std::string text;
    bool offerWithoutAdmin = false;  // UAC was declined: offer to install the non-admin part
};

// Application state + actions. Drawing lives in screens/*.cpp.
class App {
public:
    App(HWND hwnd, Args args);
    ~App();

    void Start();
    void Frame();
    bool Animating() const;
    bool CaptionHit(POINT client, const RECT& clientRect) const;
    bool ReadyForScreenshot() const;
    bool QuitRequested() const { return quit_; }
    bool CanClose() const { return !installing_; }
    void RequestQuit() { quit_ = true; }
    void Wake() const { PostMessageW(hwnd_, WM_APP_WAKE, 0, 0); }

    // ---- state read by the screens
    HWND hwnd() const { return hwnd_; }
    const Args& args() const { return args_; }
    Screen screen = Screen::Select;
    std::vector<GameEntry> games;
    std::wstring selectedKey;
    Banner selectBanner;

    fs::path target;
    std::shared_ptr<const FolderReport> report;
    InstallPlan plan;
    Options options;
    bool analyzing = false;
    Banner analysisBanner;

    InstallProgress progress;
    std::vector<std::string> installLog;
    std::optional<InstallResult> result;

    Discovery* discovery() const { return discovery_.get(); }

    // ---- actions
    void StartDiscovery();
    void BrowseFolder();
    void OpenAnalysis(const fs::path& dir);
    void BackToSelect();
    void RebuildPlan();
    void BeginInstall();
    void InstallWithoutAdmin();
    void CancelInstall();
    void OpenInExplorer(const fs::path& dir) const;
    void OpenLog() const;

private:
    void StartInstallThread();
    void PumpBackgroundResults();

    HWND hwnd_;
    Args args_;
    bool quit_ = false;
    bool autoInstallPending_ = false;

    std::unique_ptr<Discovery> discovery_;

    std::mutex mutex_;  // guards the *Pending_ fields below
    std::shared_ptr<const FolderReport> analysisPending_;
    std::optional<InstallResult> resultPending_;
    InstallProgress progressPending_;
    std::vector<std::string> logPending_;
    std::jthread analyzeThread_;
    std::jthread installThread_;
    std::atomic<bool> installing_{false};
};

// Implemented in main_win.cpp: lets the elevated copy take over the single-instance lock.
void ReleaseInstanceLock();

}  // namespace uf::ui
