// End-to-end: real payload, real file operations in a temporary "game" folder (with a Cyrillic path).
#include <doctest/doctest.h>

#include <windows.h>

#include <format>
#include <map>

#include "core/fsutil.h"
#include "core/installer.h"
#include "core/payload.h"
#include "core/plan.h"
#include "payload_manifest.gen.h"

using namespace uf;

namespace {

struct TempGame {
    fs::path root;
    fs::path dir;
    fs::path fonts;

    TempGame() {
        root = GetFolder(Folder::Temp) / std::format(L"uf-test-{}-{}", GetCurrentProcessId(), GetTickCount64());
        dir = root / L"Игры" / L"GTA San Andreas";
        fonts = root / L"fonts";
        CreateDirs(dir);
        // Minimal "game": any gta_sa.exe and the original vorbisFile.dll (= vorbisHooked.dll of the payload).
        HANDLE f = CreateFileW((dir / L"gta_sa.exe").c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, 0, nullptr);
        DWORD w = 0;
        WriteFile(f, "MZ", 2, &w, nullptr);
        CloseHandle(f);
        Payload::Instance().Extract("asi/vorbisHooked.dll", dir / L"vorbisFile.dll");
    }
    ~TempGame() {
        std::error_code ec;
        fs::remove_all(root, ec);
    }

    FolderReport Report() const {
        InspectOptions o;
        o.fontsDir = fonts;
        return Inspect(dir, o);
    }
    static Options Opts() {
        Options o;
        o.customFontsDir = true;
        o.Set(ItemId::DirectX, false);
        return o;
    }
    InstallEnv Env(int failAfter = -1) const {
        InstallEnv e;
        e.fontsDir = fonts;
        e.registerFonts = false;
        e.failAfter = failAfter;
        return e;
    }
    void Write(const fs::path& rel, const std::string& text) const {
        CreateDirs((dir / rel).parent_path());
        HANDLE f = CreateFileW((dir / rel).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
        DWORD w = 0;
        WriteFile(f, text.data(), static_cast<DWORD>(text.size()), &w, nullptr);
        CloseHandle(f);
    }
    std::string Read(const fs::path& rel) const {
        auto b = ReadFileBytes(dir / rel);
        return b ? std::string(b->begin(), b->end()) : std::string();
    }
};

// rel path -> "size:crc" (or "<dir>") for the whole tree.
std::map<std::wstring, std::string> Snapshot(const fs::path& dir) {
    std::map<std::wstring, std::string> out;
    for (auto& e : fs::recursive_directory_iterator(dir)) {
        std::wstring rel = ToLower(e.path().lexically_relative(dir).native());
        out[rel] = e.is_directory() ? "<dir>" : std::format("{}:{:08x}", e.file_size(), FileCrc32(e.path()).value_or(0));
    }
    return out;
}

}  // namespace

TEST_CASE("payload is embedded") {
    REQUIRE(Payload::Instance().ok());
}

TEST_CASE("fresh install into a bare folder, then nothing left to do") {
    TempGame g;
    FolderReport r = g.Report();
    CHECK(r.loader == LoaderKind::None);
    CHECK(r.nonAsciiPath);
    InstallPlan plan = BuildPlan(r, TempGame::Opts());
    REQUIRE_FALSE(plan.blocked);
    CHECK_FALSE(plan.needsAdmin);
    CHECK(plan.fonts.size() == std::size(gen::kFonts));

    InstallResult res = RunInstall(r.dir, plan, g.Env(), nullptr, nullptr);
    INFO(res.error);
    REQUIRE(res.ok);
    CHECK(res.replaced == 1);  // original vorbisFile.dll -> backup
    CHECK(res.fontsInstalled == static_cast<int>(std::size(gen::kFonts)));
    REQUIRE_FALSE(res.backupDir.empty());
    CHECK(FileMatches(res.backupDir / L"vorbisFile.dll", gen::kOriginalVorbisFile.size, gen::kOriginalVorbisFile.crc));
    CHECK(FileExists(res.backupDir / L"install.log"));

    for (const wchar_t* f : {L"vorbisFile.dll", L"vorbisHooked.dll", L"scripts\\global.ini", L"CLEO.asi", L"bass.dll", L"MoonLoader.asi",
                             L"lua51.dll", L"moonloader\\lib\\samp\\events.lua", L"moonloader\\lib\\imgui.lua",
                             L"moonloader\\lib\\MoonImGui.dll", L"moonloader\\config\\UltraFuck\\Settings.ini"})
        CHECK_MESSAGE(FileExists(r.dir / f), PathUtf8(f));
    CHECK(FileExists(r.dir / ToWide(gen::kScriptDest)));
    CHECK(DirExists(r.dir / L"cleo\\cleo_saves"));
    CHECK(FileExists(g.fonts / L"trebucbd.ttf"));
    CHECK_FALSE(FileExists(r.dir / L"moonloader\\lib\\lua-utf8.dll"));
    CHECK_FALSE(FileExists(r.dir / L"moonloader\\lib\\socket\\desktop.ini"));
    CHECK_FALSE(FileExists(r.dir / L"SAMPFUNCS.asi"));  // no samp.dll -> SAMPFUNCS is not installed

    FolderReport after = g.Report();
    CHECK(after.loader == LoaderKind::Silent);
    CHECK(after.ufExactInstalled);
    CHECK(after.MissingUnitCount() == 0);
    InstallPlan again = BuildPlan(after, TempGame::Opts());
    CHECK(again.Empty());
}

TEST_CASE("user settings are preserved and old script versions go to the backup") {
    TempGame g;
    g.Write(L"moonloader\\config\\UltraFuck\\Settings.ini", "[main]\nlogin=my_login\n");
    g.Write(L"moonloader\\UltraFuck 2.4.lua", "script_name('UltraFuck')\nscript_version(2.4)\n");
    FolderReport r = g.Report();
    REQUIRE(r.ufScripts.size() == 1);
    CHECK(r.ufScripts[0].version == "2.4");
    InstallResult res = RunInstall(r.dir, BuildPlan(r, TempGame::Opts()), g.Env(), nullptr, nullptr);
    INFO(res.error);
    REQUIRE(res.ok);
    CHECK(res.movedOld == 1);
    CHECK(g.Read(L"moonloader\\config\\UltraFuck\\Settings.ini") == "[main]\nlogin=my_login\n");
    CHECK_FALSE(FileExists(r.dir / L"moonloader\\UltraFuck 2.4.lua"));
    CHECK(FileExists(res.backupDir / L"moonloader\\UltraFuck 2.4.lua"));
    CHECK(FileExists(r.dir / ToWide(gen::kScriptDest)));
}

TEST_CASE("a failure in the middle rolls everything back") {
    TempGame g;
    g.Write(L"moonloader\\UltraFuck 2.4.lua", "script_name('UltraFuck')\n");
    auto before = Snapshot(g.root);
    FolderReport r = g.Report();
    InstallPlan plan = BuildPlan(r, TempGame::Opts());
    for (int failAfter : {0, 1, 40, static_cast<int>(plan.files.size()) - 1}) {
        InstallResult res = RunInstall(r.dir, plan, g.Env(failAfter), nullptr, nullptr);
        CHECK_FALSE(res.ok);
        CHECK(res.rolledBack);
        CHECK_MESSAGE(res.error.find("fail-after") != std::string::npos, res.error);
        CHECK(Snapshot(g.root) == before);
    }
}

TEST_CASE("running installer twice is idempotent") {
    TempGame g;
    FolderReport r = g.Report();
    InstallPlan plan = BuildPlan(r, TempGame::Opts());
    REQUIRE(RunInstall(r.dir, plan, g.Env(), nullptr, nullptr).ok);
    auto once = Snapshot(g.dir);
    // Re-running the very same (now stale) plan must not change anything but may skip everything.
    InstallResult second = RunInstall(r.dir, plan, g.Env(), nullptr, nullptr);
    REQUIRE(second.ok);
    CHECK(second.installed == 0);
    CHECK(second.replaced == 0);
    auto twice = Snapshot(g.dir);
    for (auto& [k, v] : twice)
        if (k.find(L"uf-installer-backup") == std::wstring::npos) CHECK(once[k] == v);
}
