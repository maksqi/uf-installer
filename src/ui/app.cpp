#include "ui/app.h"

#include <shellapi.h>
#include <shobjidl.h>

#include <format>

#include "core/elevation.h"
#include "core/fonts.h"
#include "core/fsutil.h"
#include "core/i18n.h"
#include "core/log.h"
#include "ui/screens/screens.h"
#include "ui/widgets.h"

namespace uf::ui {

std::string WindowTitle() { return T("UltraFuck — установка", "UltraFuck Setup"); }

App::App(HWND hwnd, Args args) : hwnd_(hwnd), args_(std::move(args)) {
    options = DecodeOptions(args_.opts);
    // Fresh start: refresh the libraries by default (only files that differ from the bundle are written).
    if (args_.opts.empty()) options.overwriteLibs = true;
    themeFollowsSystem = args_.theme.empty();
    if (args_.fontsDir) options.customFontsDir = true;
}

App::~App() {
    installThread_ = {};
    analyzeThread_ = {};
    discovery_.reset();
}

void App::Start() {
    if (args_.target) {
        autoInstallPending_ = args_.install;
        OpenAnalysis(*args_.target);
    } else {
        StartDiscovery();
    }
}

void App::StartDiscovery() {
    if (discovery_) return;
    DiscoveryConfig cfg;
    cfg.inspect.fontsDir = args_.fontsDir;
    cfg.inspect.simulateMissingFonts = args_.simulateMissingFonts;
    cfg.inspect.simulateMissingD3dx9 = args_.simulateMissingD3dx9;
    cfg.diskScan = !args_.noScan;
    cfg.arizonaRoot = args_.arizonaRoot;
    cfg.arizonaSettings = args_.arizonaSettings;
    discovery_ = std::make_unique<Discovery>(std::move(cfg));
    discovery_->Start([this] { Wake(); });
}

void App::OpenAnalysis(const fs::path& dir) {
    target = CanonicalPath(dir);
    screen = Screen::Analysis;
    analyzing = true;
    report.reset();
    analysisBanner = {};
    const ArizonaInfo* known = discovery_ ? &discovery_->Arizona() : nullptr;
    analyzeThread_ = std::jthread([this, dir = target, known](std::stop_token stop) {
        ArizonaInfo local;
        if (!known) local = LoadArizonaInfo(args_.arizonaRoot, args_.arizonaSettings);
        InspectOptions io;
        io.fontsDir = args_.fontsDir;
        io.simulateMissingFonts = args_.simulateMissingFonts;
        io.simulateMissingD3dx9 = args_.simulateMissingD3dx9;
        io.arizona = known ? known : &local;
        auto r = std::make_shared<FolderReport>(Inspect(dir, io));
        if (stop.stop_requested()) return;
        {
            std::lock_guard lock(mutex_);
            analysisPending_ = std::move(r);
        }
        Wake();
    });
}

void App::BackToSelect() {
    screen = Screen::Select;
    analysisBanner = {};
    StartDiscovery();
}

void App::RebuildPlan() {
    if (report) plan = BuildPlan(*report, options);
}

void App::PumpBackgroundResults() {
    bool gotReport = false;
    {
        std::lock_guard lock(mutex_);
        if (analysisPending_) {
            report = std::move(analysisPending_);
            analysisPending_.reset();
            analyzing = false;
            gotReport = true;
        }
        progress = progressPending_;
        if (!logPending_.empty()) {
            installLog.insert(installLog.end(), logPending_.begin(), logPending_.end());
            logPending_.clear();
        }
        if (resultPending_) {
            result = std::move(resultPending_);
            resultPending_.reset();
            screen = Screen::Done;
            if (discovery_) discovery_->Refresh(target);
        }
    }
    if (gotReport) {
        RebuildPlan();
        if (autoInstallPending_) {
            autoInstallPending_ = false;
            if (!args_.planHash || *args_.planHash == plan.Hash())
                BeginInstall();
            else
                analysisBanner = {Severity::Warning,
                                  T("Пока запрашивались права администратора, состояние папки изменилось. Проверьте список и "
                                    "нажмите «Установить» ещё раз.",
                                    "The folder changed while administrator rights were requested. Check the list and press "
                                    "\"Install\" again."),
                                  false};
        }
    }
}

void App::BrowseFolder() {
    IFileOpenDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg)))) return;
    DWORD flags = 0;
    dlg->GetOptions(&flags);
    dlg->SetOptions(flags | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    dlg->SetTitle(ToWide(T("Выберите папку с GTA San Andreas (где лежит gta_sa.exe)",
                           "Select the GTA San Andreas folder (the one with gta_sa.exe)")).c_str());
    std::optional<fs::path> picked;
    if (SUCCEEDED(dlg->Show(hwnd_))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item))) {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path) picked = fs::path(path);
            CoTaskMemFree(path);
            item->Release();
        }
    }
    dlg->Release();
    if (!picked) return;
    StartDiscovery();
    if (auto game = discovery_->AddManual(*picked)) {
        selectBanner = {};
        selectedKey = PathKey(*game);
        OpenAnalysis(*game);
    } else {
        selectBanner = {Severity::Error,
                        F("В папке «{}» и её подпапках нет gta_sa.exe. Укажите папку, в которой лежит игра.",
                          "There is no gta_sa.exe in \"{}\" or its subfolders. Select the folder the game is in.", PathUtf8(*picked)),
                        false};
    }
}

void App::BeginInstall() {
    if (!report || installing_ || plan.blocked || plan.Empty()) return;
    analysisBanner = {};
    if (plan.needsAdmin && !IsProcessElevated() && !args_.noElevate) {
        if (args_.elevated) {
            // We already are the "elevated" copy but did not get admin rights (e.g. UAC is off for a standard user).
            Options reduced = options;
            reduced.Set(ItemId::Fonts, false);
            reduced.Set(ItemId::DirectX, false);
            InstallPlan alt = BuildPlan(*report, reduced);
            analysisBanner = {Severity::Error,
                              T("Windows не выдала права администратора. Запустите установщик от имени администратора "
                                "(правый клик → «Запуск от имени администратора»).",
                                "Windows did not grant administrator rights. Run the installer as administrator "
                                "(right click → \"Run as administrator\")."),
                              !alt.needsAdmin && !alt.Empty()};
            return;
        }
        RECT rc{};
        GetWindowRect(hwnd_, &rc);
        // The elevated copy keeps the language and theme the user sees now.
        Args base = args_;
        base.lang = LangCode(CurrentLang());
        base.theme = ThemeName(CurrentTheme());
        std::wstring params = BuildElevatedParameters(base, report->dir, EncodeOptions(options), plan.Hash(), true,
                                                      std::pair{static_cast<int>(rc.left), static_cast<int>(rc.top)});
        log::Info("Requesting elevation: {}", ToUtf8(params));
        unsigned long err = 0;
        switch (RelaunchElevated(hwnd_, params, &err)) {
            case ElevateResult::Started:
                ReleaseInstanceLock();
                quit_ = true;
                return;
            case ElevateResult::Cancelled: {
                Options reduced = options;
                reduced.Set(ItemId::Fonts, false);
                reduced.Set(ItemId::DirectX, false);
                InstallPlan alt = BuildPlan(*report, reduced);
                Banner b{Severity::Warning, T("Права администратора не получены. ", "Administrator rights were not granted. "), false};
                if (!alt.needsAdmin && !alt.Empty()) {
                    b.text += T("Можно установить всё остальное — без шрифтов и DirectX.",
                                "Everything else can still be installed, without the fonts and DirectX.");
                    b.offerWithoutAdmin = true;
                } else {
                    b.text += T("Без них установить в эту папку не получится.", "Installing into this folder is not possible without them.");
                }
                analysisBanner = b;
                return;
            }
            case ElevateResult::Failed:
                analysisBanner = {Severity::Error,
                                  T("Не удалось запросить права администратора: ", "Could not request administrator rights: ") +
                                      Win32ErrorText(err),
                                  false};
                return;
        }
    }
    StartInstallThread();
}

void App::InstallWithoutAdmin() {
    options.Set(ItemId::Fonts, false);
    options.Set(ItemId::DirectX, false);
    RebuildPlan();
    analysisBanner = {};
    if (!plan.needsAdmin) StartInstallThread();
}

void App::StartInstallThread() {
    screen = Screen::Progress;
    installLog.clear();
    progress = {};
    result.reset();
    {
        std::lock_guard lock(mutex_);
        progressPending_ = {};
        logPending_.clear();
    }
    installing_ = true;
    InstallEnv env;
    env.fontsDir = args_.fontsDir ? *args_.fontsDir : SystemFontsDir();
    env.registerFonts = !args_.fontsDir;
    env.failAfter = args_.failAfter;
    env.throttleMs = args_.throttleMs;
    installThread_ = std::jthread([this, env, planCopy = plan, dir = report->dir](std::stop_token stop) {
        InstallResult r = RunInstall(
            dir, planCopy, env,
            [this](const InstallProgress& p) {
                {
                    std::lock_guard lock(mutex_);
                    progressPending_ = p;
                }
                Wake();
            },
            [this](const std::string& line) {
                {
                    std::lock_guard lock(mutex_);
                    logPending_.push_back(line);
                }
                Wake();
            },
            stop);
        {
            std::lock_guard lock(mutex_);
            resultPending_ = std::move(r);
        }
        installing_ = false;
        Wake();
    });
}

void App::CancelInstall() { installThread_.request_stop(); }

void App::OpenInExplorer(const fs::path& dir) const {
    ShellExecuteW(hwnd_, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void App::OpenLog() const {
    fs::path file = log::File();
    if (!file.empty()) ShellExecuteW(hwnd_, L"open", L"notepad.exe", file.c_str(), nullptr, SW_SHOWNORMAL);
}

bool App::Animating() const {
    if (installing_ || analyzing || screen == Screen::Progress) return true;
    if (screen == Screen::Select && discovery_) {
        if (!discovery_->HintsDone() || discovery_->ScanRunning()) return true;
        for (const GameEntry& e : games)
            if (!e.report) return true;
    }
    return false;
}

bool App::CaptionHit(POINT pt, const RECT& rc) const {
    return pt.y >= 0 && pt.y < S(kTitleBarHeight) && pt.x < rc.right - S(kCaptionButtonWidth * 2 + kTitleToolsWidth);
}

bool App::ReadyForScreenshot() const {
    switch (screen) {
        case Screen::Select: {
            if (!discovery_ || !discovery_->HintsDone() || discovery_->ScanRunning()) return false;
            for (const GameEntry& e : games)
                if (!e.report) return false;
            return true;
        }
        case Screen::Analysis: return !analyzing;
        case Screen::Progress:
        case Screen::Done: return true;
    }
    return true;
}

void App::ToggleTheme() {
    themeFollowsSystem = false;
    pendingTheme_ = CurrentTheme() == Theme::Dark ? Theme::Light : Theme::Dark;
    Wake();
}

void App::OnSystemThemeChanged() {
    if (!themeFollowsSystem) return;
    if (Theme t = SystemTheme(); t != CurrentTheme()) {
        pendingTheme_ = t;
        Wake();
    }
}

void App::SetLanguage(Lang lang) {
    if (lang == CurrentLang()) return;
    log::Info("Language switched to {}", LangCode(lang));
    SetLang(lang);
    SetWindowTextW(hwnd_, ToWide(WindowTitle()).c_str());
    // Texts of the plan are built in the current language.
    if (report && !analyzing) RebuildPlan();
}

void App::Frame() {
    if (pendingTheme_) {
        ApplyTheme(*pendingTheme_, hwnd_);
        pendingTheme_.reset();
    }
    PumpBackgroundResults();
    if (discovery_) games = discovery_->Snapshot();

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::Begin("##root", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollWithMouse);
    DrawTitleBar(*this);
    switch (screen) {
        case Screen::Select: DrawSelectScreen(*this); break;
        case Screen::Analysis: DrawAnalysisScreen(*this); break;
        case Screen::Progress: DrawProgressScreen(*this); break;
        case Screen::Done: DrawDoneScreen(*this); break;
    }
    ImGui::End();
}

}  // namespace uf::ui
