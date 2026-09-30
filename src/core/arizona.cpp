#include "core/arizona.h"

#include <nlohmann/json.hpp>

#include "core/fsutil.h"
#include "core/registry.h"

namespace uf {

namespace {

constexpr wchar_t kLauncherExe[] = L"Arizona Games Launcher.exe";
constexpr wchar_t kUninstallKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall";

fs::path DirFromIconOrCommand(std::wstring s) {
    // DisplayIcon: "C:\...\Arizona Games Launcher.exe,0" (quotes optional)
    if (auto comma = s.rfind(L','); comma != std::wstring::npos && comma > s.rfind(L'\\')) s.resize(comma);
    if (!s.empty() && s.front() == L'"') {
        auto end = s.find(L'"', 1);
        s = s.substr(1, end == std::wstring::npos ? std::wstring::npos : end - 1);
    }
    return fs::path(s).parent_path();
}

}  // namespace

std::optional<fs::path> FindArizonaLauncherDir() {
    struct View {
        HKEY root;
        REGSAM sam;
    };
    for (View v : {View{HKEY_CURRENT_USER, 0}, View{HKEY_LOCAL_MACHINE, KEY_WOW64_64KEY}, View{HKEY_LOCAL_MACHINE, KEY_WOW64_32KEY}}) {
        for (const std::wstring& sub : reg::SubKeys(v.root, kUninstallKey, v.sam)) {
            std::wstring key = std::wstring(kUninstallKey) + L"\\" + sub;
            auto name = reg::ReadString(v.root, key, L"DisplayName", v.sam);
            if (!name || !IStartsWith(*name, L"Arizona Games Launcher")) continue;
            std::vector<fs::path> candidates;
            if (auto loc = reg::ReadString(v.root, key, L"InstallLocation", v.sam); loc && !loc->empty()) candidates.emplace_back(*loc);
            if (auto icon = reg::ReadString(v.root, key, L"DisplayIcon", v.sam); icon && !icon->empty())
                candidates.push_back(DirFromIconOrCommand(*icon));
            if (auto un = reg::ReadString(v.root, key, L"UninstallString", v.sam); un && !un->empty())
                candidates.push_back(DirFromIconOrCommand(*un));
            for (const fs::path& c : candidates)
                if (FileExists(c / kLauncherExe)) return c;
        }
    }
    fs::path fallback = GetFolder(Folder::LocalAppData) / L"Programs" / L"Arizona Games Launcher";
    if (FileExists(fallback / kLauncherExe)) return fallback;
    return std::nullopt;
}

std::string ArizonaTitle(std::string_view id) {
    std::string base(id);
    bool staging = false;
    if (auto p = base.find("_staging"); p != std::string::npos) {
        base.resize(p);
        staging = true;
    }
    std::string title = base == "arizona" ? "Arizona RP" : base == "rodina" ? "Rodina RP" : base == "village" ? "Village" : base;
    return staging ? title + " (тестовый сервер)" : title;
}

std::vector<ArizonaGame> ParseArizonaSettings(std::string_view text, const fs::path& launcherDir) {
    using nlohmann::json;
    json root = json::parse(text.begin(), text.end(), nullptr, /*allow_exceptions=*/false);
    std::vector<ArizonaGame> games;
    for (const char* id : kArizonaGameIds) {
        ArizonaGame g;
        g.id = id;
        g.title = ArizonaTitle(id);
        if (root.is_object()) {
            auto it = root.find(id);
            if (it != root.end() && it->is_object()) {
                auto path = it->find("gamePath");
                if (path != it->end() && path->is_string() && !path->get<std::string>().empty()) {
                    g.dir = fs::path(ToWide(path->get<std::string>()));
                    g.customPath = true;
                }
                auto options = it->find("options");
                if (options != it->end() && options->is_array()) {
                    for (const json& o : *options) {
                        if (o.is_object() && o.value("id", std::string()) == "autoClean") {
                            auto value = o.find("value");
                            g.autoClean = value != o.end() && value->is_boolean() && value->get<bool>();
                        }
                    }
                }
            }
        }
        if (g.dir.empty() && !launcherDir.empty()) g.dir = launcherDir / L"bin" / ToWide(id);
        if (!g.dir.empty()) games.push_back(std::move(g));
    }
    return games;
}

ArizonaInfo LoadArizonaInfo(const std::optional<fs::path>& launcherOverride, const std::optional<fs::path>& settingsOverride) {
    ArizonaInfo info;
    if (launcherOverride)
        info.launcherDir = *launcherOverride;
    else if (auto dir = FindArizonaLauncherDir())
        info.launcherDir = *dir;
    info.settingsFile = settingsOverride ? *settingsOverride : GetFolder(Folder::RoamingAppData) / L"arizona-launcher" / L"settings.json";
    std::string text;
    if (auto bytes = ReadFileBytes(info.settingsFile, 16 << 20)) text.assign(bytes->begin(), bytes->end());
    if (info.launcherDir.empty() && text.empty()) return info;
    info.games = ParseArizonaSettings(text, info.launcherDir);
    return info;
}

std::optional<std::string> ArizonaIdFromLayout(const fs::path& gameDir) {
    fs::path dir = CanonicalPath(gameDir);
    fs::path bin = dir.parent_path();
    if (!IEquals(bin.filename().native(), L"bin")) return std::nullopt;
    if (!FileExists(bin.parent_path() / kLauncherExe)) return std::nullopt;
    return ToUtf8(ToLower(dir.filename().native()));
}

bool IsArizonaManagedFile(const fs::path& relPath) {
    static const wchar_t* kManaged[] = {L"vorbisfile.dll", L"vorbishooked.dll", L"vorbis.dll", L"ogg.dll", L"eax.dll",
                                        L"bass.dll",       L"cleo.asi",         L"sampfuncs.asi", L"samp.dll", L"gta_sa.exe"};
    std::wstring rel = ToLower(relPath.lexically_normal().native());
    for (const wchar_t* m : kManaged)
        if (rel == m) return true;
    return rel.starts_with(L"cleo\\") && rel.ends_with(L".cleo");
}

}  // namespace uf
