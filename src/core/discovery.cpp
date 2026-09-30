#include "core/discovery.h"

#include <windows.h>
#include <objbase.h>
#include <shlobj.h>
#include <shobjidl.h>

#include "core/fsutil.h"
#include "core/log.h"
#include "core/registry.h"
#include "core/i18n.h"

namespace uf {

namespace {

constexpr int kMaxScanDepth = 10;

std::int64_t NowMs() { return static_cast<std::int64_t>(GetTickCount64()); }

fs::path DirFromCommand(std::wstring s) {
    if (auto comma = s.rfind(L','); comma != std::wstring::npos && comma > s.rfind(L'\\')) s.resize(comma);
    if (!s.empty() && s.front() == L'"') {
        auto end = s.find(L'"', 1);
        s = s.substr(1, end == std::wstring::npos ? std::wstring::npos : end - 1);
    } else if (auto exe = ToLower(s).find(L".exe "); exe != std::wstring::npos) {
        s.resize(exe + 4);
    }
    return fs::path(s).parent_path();
}

bool SkipDir(const std::wstring& parent, const std::wstring& name, int depth) {
    static const wchar_t* kAnyDepth[] = {L"$recycle.bin", L"system volume information", L"$winreagent", L"$sysreset",
                                         L"$windows.~bt", L"$windows.~ws", L"config.msi", L"node_modules", L".git",
                                         L".svn", L".hg", L".vs", L"__pycache__", L"winsxs", L".nuget", L".gradle",
                                         L".cargo", L".rustup", L".npm", L"site-packages", L".m2", L"go-build"};
    static const wchar_t* kRoot[] = {L"windows", L"windows.old", L"perflogs", L"msocache", L"recovery", L"$getcurrent"};
    static const wchar_t* kSuffixes[] = {
        L"\\programdata\\microsoft",         L"\\programdata\\package cache",       L"\\programdata\\packages",
        L"\\appdata\\local\\microsoft",      L"\\appdata\\local\\packages",         L"\\appdata\\local\\temp",
        L"\\appdata\\roaming\\microsoft",    L"\\appdata\\local\\google",           L"\\appdata\\local\\mozilla",
        L"\\appdata\\local\\nvidia",         L"\\appdata\\local\\npm-cache",        L"\\appdata\\local\\pip",
        L"\\appdata\\local\\jetbrains",      L"\\appdata\\local\\yandex",           L"\\appdata\\roaming\\opera software",
        L"\\program files\\windowsapps",     L"\\program files\\windows defender",  L"\\program files\\common files",
        L"\\program files (x86)\\common files", L"\\program files\\microsoft visual studio",
        L"\\program files (x86)\\microsoft visual studio", L"\\program files (x86)\\windows kits",
        L"\\program files\\windows kits",    L"\\program files (x86)\\microsoft sdks",
    };
    std::wstring lower = ToLower(name);
    for (const wchar_t* s : kAnyDepth)
        if (lower == s) return true;
    if (depth == 0)
        for (const wchar_t* s : kRoot)
            if (lower == s) return true;
    std::wstring full = ToLower(parent) + L"\\" + lower;
    for (const wchar_t* s : kSuffixes)
        if (full.ends_with(s)) return true;
    return false;
}

template <typename F>
void ForEachShortcut(const fs::path& dir, int depth, F&& fn) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileExW((dir / L"*").c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, 0);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        std::wstring name = fd.cFileName;
        if (name == L"." || name == L"..") continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (depth > 0 && !(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) ForEachShortcut(dir / name, depth - 1, fn);
        } else if (IEndsWith(name, L".lnk")) {
            fn(dir / name);
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

std::vector<fs::path> ShortcutDirs(const fs::path& lnk) {
    std::vector<fs::path> out;
    IShellLinkW* link = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link)))) return out;
    IPersistFile* file = nullptr;
    if (SUCCEEDED(link->QueryInterface(IID_PPV_ARGS(&file)))) {
        if (SUCCEEDED(file->Load(lnk.c_str(), STGM_READ))) {
            wchar_t buf[MAX_PATH * 2] = {};
            wchar_t expanded[MAX_PATH * 2] = {};
            if (SUCCEEDED(link->GetPath(buf, MAX_PATH * 2, nullptr, SLGP_RAWPATH)) && buf[0]) {
                ExpandEnvironmentStringsW(buf, expanded, MAX_PATH * 2);
                out.push_back(fs::path(expanded).parent_path());
            }
            if (SUCCEEDED(link->GetWorkingDirectory(buf, MAX_PATH * 2)) && buf[0]) {
                ExpandEnvironmentStringsW(buf, expanded, MAX_PATH * 2);
                out.emplace_back(expanded);
            }
        }
        file->Release();
    }
    link->Release();
    return out;
}

}  // namespace

std::optional<fs::path> ResolveGameDir(const fs::path& dir, int depth) {
    if (HasGtaExe(dir)) return dir;
    if (depth <= 0) return std::nullopt;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileExW((dir / L"*").c_str(), FindExInfoBasic, &fd, FindExSearchLimitToDirectories, nullptr, 0);
    if (h == INVALID_HANDLE_VALUE) return std::nullopt;
    std::optional<fs::path> found;
    do {
        std::wstring name = fd.cFileName;
        if (name == L"." || name == L".." || !(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        found = ResolveGameDir(dir / name, depth - 1);
    } while (!found && FindNextFileW(h, &fd));
    FindClose(h);
    return found;
}

std::string DescribeSources(std::uint32_t s) {
    std::vector<std::string> parts;
    if (s & kSrcArizona) parts.push_back("Arizona Launcher");
    if (s & kSrcSampRegistry) parts.push_back("SA-MP");
    if (s & kSrcManual) parts.push_back(T("выбрано вручную", "picked manually"));
    if (s & kSrcShortcut) parts.push_back(T("ярлык", "shortcut"));
    if (s & kSrcUninstall) parts.push_back(T("установленные программы", "installed programs"));
    if (s & (kSrcAppCompat | kSrcMuiCache)) parts.push_back(T("история запусков", "launch history"));
    if (s & kSrcDiskScan) parts.push_back(T("поиск по дискам", "drive scan"));
    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i) out += (i ? " · " : "") + parts[i];
    return out;
}

Discovery::Discovery(DiscoveryConfig config) : config_(std::move(config)) {}

Discovery::~Discovery() {
    scanStop_.request_stop();
    for (auto& t : threads_) t.request_stop();
    inspectCv_.notify_all();
    threads_.clear();
}

void Discovery::Notify() {
    if (onChange_) onChange_();
}

void Discovery::Start(std::function<void()> onChange) {
    onChange_ = std::move(onChange);
    arizona_ = LoadArizonaInfo(config_.arizonaRoot, config_.arizonaSettings);
    config_.inspect.arizona = &arizona_;

    threads_.emplace_back([this](std::stop_token stop) { InspectLoop(stop); });
    threads_.emplace_back([this](std::stop_token) {
        HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        std::int64_t t0 = NowMs();
        CollectHints();
        log::Info("Discovery: hints collected in {} ms", NowMs() - t0);
        if (SUCCEEDED(co)) CoUninitialize();
        hintsDone_ = true;
        Notify();
    });

    if (!config_.diskScan) return;
    std::vector<std::wstring> roots;
    wchar_t drives[512];
    DWORD n = GetLogicalDriveStringsW(512, drives);
    for (const wchar_t* d = drives; n && *d; d += wcslen(d) + 1)
        if (GetDriveTypeW(d) == DRIVE_FIXED) roots.emplace_back(d);
    scanStartMs_ = NowMs();
    scanThreadsLeft_ = static_cast<int>(roots.size());
    if (roots.empty()) scanEndMs_ = NowMs();
    for (const std::wstring& root : roots)
        threads_.emplace_back([this, root](std::stop_token) { ScanDrive(root, scanStop_.get_token()); });
}

void Discovery::StopDiskScan() { scanStop_.request_stop(); }

double Discovery::ScanSeconds() const {
    if (scanStartMs_ == 0) return 0;
    std::int64_t end = scanEndMs_ ? scanEndMs_.load() : NowMs();
    return (end - scanStartMs_) / 1000.0;
}

void Discovery::Offer(const fs::path& dirIn, std::uint32_t source, const std::string& arizonaId) {
    if (dirIn.empty() || !HasGtaExe(dirIn)) return;
    fs::path dir = CanonicalPath(dirIn);
    std::wstring key = ToLower(dir.native());
    {
        std::lock_guard lock(mutex_);
        if (auto it = index_.find(key); it != index_.end()) {
            GameEntry& e = entries_[it->second];
            if ((e.sources & source) == source && (arizonaId.empty() || !e.arizonaId.empty())) return;
            e.sources |= source;
            if (!arizonaId.empty() && e.arizonaId.empty()) {
                e.arizonaId = arizonaId;
                e.title = ArizonaTitle(arizonaId);
            }
        } else {
            GameEntry e;
            e.dir = dir;
            e.sources = source;
            e.arizonaId = arizonaId;
            if (e.arizonaId.empty())
                if (auto id = ArizonaIdFromLayout(dir)) e.arizonaId = *id;
            e.title = e.arizonaId.empty() ? PathUtf8(dir.filename()) : ArizonaTitle(e.arizonaId);
            index_[key] = entries_.size();
            inspectQueue_.push_back(entries_.size());
            entries_.push_back(std::move(e));
            log::Info("Discovery: found {} ({})", PathUtf8(dir), DescribeSources(source));
        }
    }
    inspectCv_.notify_one();
    Notify();
}

void Discovery::CollectHints() {
    if (auto exe = reg::ReadString(HKEY_CURRENT_USER, L"Software\\SAMP", L"gta_sa_exe"))
        Offer(fs::path(*exe).parent_path(), kSrcSampRegistry);

    for (const ArizonaGame& g : arizona_.games) Offer(g.dir, kSrcArizona, g.id);

    auto fromExeNames = [&](HKEY root, const std::wstring& key, REGSAM view, std::uint32_t src) {
        for (const std::wstring& name : reg::ValueNames(root, key, view)) {
            std::wstring lower = ToLower(name);
            auto pos = lower.find(L".exe");
            if (pos == std::wstring::npos) continue;
            fs::path exe(name.substr(0, pos + 4));
            std::wstring file = ToLower(exe.filename().native());
            if (file == L"gta_sa.exe" || file == L"samp.exe") Offer(exe.parent_path(), src);
        }
    };
    const std::wstring appCompat = L"Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags";
    fromExeNames(HKEY_CURRENT_USER, appCompat + L"\\Layers", 0, kSrcAppCompat);
    fromExeNames(HKEY_LOCAL_MACHINE, appCompat + L"\\Layers", KEY_WOW64_64KEY, kSrcAppCompat);
    fromExeNames(HKEY_CURRENT_USER, appCompat + L"\\Compatibility Assistant\\Store", 0, kSrcAppCompat);
    fromExeNames(HKEY_CURRENT_USER, L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\Shell\\MuiCache", 0, kSrcMuiCache);

    const std::wstring uninstall = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
    struct View {
        HKEY root;
        REGSAM sam;
    };
    for (View v : {View{HKEY_CURRENT_USER, 0}, View{HKEY_LOCAL_MACHINE, KEY_WOW64_64KEY}, View{HKEY_LOCAL_MACHINE, KEY_WOW64_32KEY}}) {
        for (const std::wstring& sub : reg::SubKeys(v.root, uninstall, v.sam)) {
            std::wstring key = uninstall + L"\\" + sub;
            auto name = reg::ReadString(v.root, key, L"DisplayName", v.sam);
            if (!name) continue;
            std::wstring lower = ToLower(*name);
            if (lower.find(L"san andreas") == std::wstring::npos && lower.find(L"gta") == std::wstring::npos &&
                lower.find(L"sa-mp") == std::wstring::npos && lower.find(L"samp") == std::wstring::npos)
                continue;
            if (auto loc = reg::ReadString(v.root, key, L"InstallLocation", v.sam); loc && !loc->empty()) Offer(*loc, kSrcUninstall);
            if (auto icon = reg::ReadString(v.root, key, L"DisplayIcon", v.sam); icon && !icon->empty())
                Offer(DirFromCommand(*icon), kSrcUninstall);
            if (auto un = reg::ReadString(v.root, key, L"UninstallString", v.sam); un && !un->empty())
                Offer(DirFromCommand(*un), kSrcUninstall);
        }
    }

    for (Folder f : {Folder::Desktop, Folder::PublicDesktop, Folder::StartMenu, Folder::CommonStartMenu}) {
        fs::path dir = GetFolder(f);
        if (dir.empty()) continue;
        ForEachShortcut(dir, 3, [&](const fs::path& lnk) {
            for (const fs::path& target : ShortcutDirs(lnk)) Offer(target, kSrcShortcut);
        });
    }
}

void Discovery::ScanDrive(std::wstring root, std::stop_token stop) {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    while (!root.empty() && root.back() == L'\\') root.pop_back();
    std::int64_t t0 = NowMs();
    ScanDir(root, 0, stop);
    log::Info("Discovery: drive {} scanned in {} ms{}", ToUtf8(root), NowMs() - t0, stop.stop_requested() ? " (stopped)" : "");
    if (--scanThreadsLeft_ == 0) {
        scanEndMs_ = NowMs();
        Notify();
    }
}

void Discovery::ScanDir(const std::wstring& dir, int depth, std::stop_token& stop) {
    if (stop.stop_requested()) return;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileExW((dir + L"\\*").c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr,
                                FIND_FIRST_EX_LARGE_FETCH);
    if (h == INVALID_HANDLE_VALUE) return;
    std::vector<std::wstring> subdirs;
    bool hasGame = false;
    constexpr DWORD kSkipAttrs = FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_OFFLINE | 0x00040000 /*RECALL_ON_OPEN*/ |
                                 0x00400000 /*RECALL_ON_DATA_ACCESS*/;
    do {
        const wchar_t* name = fd.cFileName;
        if (name[0] == L'.' && (name[1] == 0 || (name[1] == L'.' && name[2] == 0))) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (fd.dwFileAttributes & kSkipAttrs) continue;
            if (!SkipDir(dir, name, depth)) subdirs.emplace_back(name);
        } else if (!hasGame && IEquals(name, L"gta_sa.exe")) {
            hasGame = true;
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    ++dirsScanned_;
    if (hasGame) {
        Offer(fs::path(dir), kSrcDiskScan);
        return;  // do not descend into a game folder
    }
    if (depth >= kMaxScanDepth) return;
    for (const std::wstring& sub : subdirs) {
        if (stop.stop_requested()) return;
        ScanDir(dir + L"\\" + sub, depth + 1, stop);
    }
}

void Discovery::InspectLoop(std::stop_token stop) {
    for (;;) {
        std::size_t idx = 0;
        fs::path dir;
        {
            std::unique_lock lock(mutex_);
            if (!inspectCv_.wait(lock, stop, [&] { return !inspectQueue_.empty(); })) return;
            idx = inspectQueue_.front();
            inspectQueue_.pop_front();
            dir = entries_[idx].dir;
        }
        auto report = std::make_shared<FolderReport>(Inspect(dir, config_.inspect));
        {
            std::lock_guard lock(mutex_);
            entries_[idx].report = std::move(report);
        }
        Notify();
    }
}

std::optional<fs::path> Discovery::AddManual(const fs::path& dir) {
    auto game = ResolveGameDir(dir);
    if (!game) return std::nullopt;
    Offer(*game, kSrcManual);
    return CanonicalPath(*game);
}

void Discovery::Refresh(const fs::path& dir) {
    std::wstring key = PathKey(dir);
    {
        std::lock_guard lock(mutex_);
        auto it = index_.find(key);
        if (it == index_.end()) return;
        entries_[it->second].report.reset();
        inspectQueue_.push_back(it->second);
    }
    inspectCv_.notify_one();
    Notify();
}

std::vector<GameEntry> Discovery::Snapshot() const {
    std::lock_guard lock(mutex_);
    return entries_;
}

}  // namespace uf
