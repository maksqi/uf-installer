#include <doctest/doctest.h>

#include <windows.h>
#include <shellapi.h>

#include <cstring>

#include "core/args.h"
#include "core/arizona.h"
#include "core/elevation.h"
#include "core/inspect.h"
#include "core/known.h"
#include "core/pe.h"
#include "core/plan.h"
#include "core/versions.h"
#include "payload_manifest.gen.h"

using namespace uf;

TEST_CASE("decimal versions compare like Lua numbers") {
    CHECK(CompareDecimalVersions("2.4", "2.36") > 0);
    CHECK(CompareDecimalVersions("2.36", "2.4") < 0);
    CHECK(CompareDecimalVersions("2.36", "2.36") == 0);
    CHECK(CompareDecimalVersions("2.40", "2.4") == 0);
    CHECK(CompareDecimalVersions("10.0", "9.99") > 0);
    CHECK(CompareDecimalVersions("2.4-fix", "2.4") == 0);
    CHECK(CompareDecimalVersions("1.2.10", "1.2.9") > 0);
    CHECK(MoonLoaderMajor("026.5-beta") == 26);
    CHECK(MoonLoaderMajor("025") == 25);
    CHECK(MoonLoaderMajor("") == -1);
}

TEST_CASE("UltraFuck script file names") {
    auto a = ParseUfScriptName(L"UltraFuck 2.4.lua");
    REQUIRE(a);
    CHECK(a->version == "2.4");
    CHECK(a->ext == L"lua");

    auto b = ParseUfScriptName(L"UltraFuck_2.36.luac");
    REQUIRE(b);
    CHECK(b->version == "2.36");
    CHECK(b->ext == L"luac");

    auto c = ParseUfScriptName(L"UltraFuck_2.4-fix.aluac");
    REQUIRE(c);
    CHECK(c->version == "2.4");
    CHECK(c->suffix == "-fix");
    CHECK(c->ext == L"aluac");

    auto d = ParseUfScriptName(L"ultrafuck.LUA");
    REQUIRE(d);
    CHECK(d->version.empty());

    CHECK_FALSE(ParseUfScriptName(L"UltraFuckHelper.lua"));
    CHECK_FALSE(ParseUfScriptName(L"UltraFuck 2.4.lua.bak"));
    CHECK_FALSE(ParseUfScriptName(L"my UltraFuck.lua"));

    CHECK(LuaDeclaresUltraFuck("script_name('UltraFuck')\nscript_version(2.4)"));
    CHECK(LuaDeclaresUltraFuck("script_name(\"ULTRAFUCK\")"));
    CHECK_FALSE(LuaDeclaresUltraFuck("script_name('Other')"));
}

TEST_CASE("SA-MP versions") {
    CHECK(SampVersionFromEntryPoint(0x31DF13) == SampVersion::R1);
    CHECK(SampVersionFromEntryPoint(0xCC4D0) == SampVersion::R3_1);
    CHECK(SampVersionFromEntryPoint(0x12345) == SampVersion::Unknown);
    CHECK(SampVersionsFromText("0.3.7-R1") == std::vector{SampVersion::R1});
    CHECK(SampVersionsFromText("0.3.7 R3-1") == std::vector{SampVersion::R3_1});
    CHECK(SampVersionsFromText(gen::kSampfuncsTarget) == std::vector{SampVersion::R1});
}

TEST_CASE("PE parsing and VA mapping") {
    std::vector<std::uint8_t> img(0x400, 0);
    img[0] = 'M';
    img[1] = 'Z';
    std::uint32_t peOff = 0x80;
    std::memcpy(&img[0x3C], &peOff, 4);
    std::memcpy(&img[peOff], "PE\0\0", 4);
    std::uint16_t machine = 0x14C, sections = 1, optSize = 0xE0, magic = 0x10B;
    std::memcpy(&img[peOff + 4], &machine, 2);
    std::memcpy(&img[peOff + 6], &sections, 2);
    std::memcpy(&img[peOff + 20], &optSize, 2);
    std::size_t opt = peOff + 24;
    std::memcpy(&img[opt], &magic, 2);
    std::uint32_t entry = 0x31DF13, base = 0x400000;
    std::memcpy(&img[opt + 16], &entry, 4);
    std::memcpy(&img[opt + 28], &base, 4);
    std::size_t sec = opt + optSize;
    std::uint32_t vsize = 0x2000, va = 0x1000, rawSize = 0x1000, rawOff = 0x400;
    std::memcpy(&img[sec + 8], &vsize, 4);
    std::memcpy(&img[sec + 12], &va, 4);
    std::memcpy(&img[sec + 16], &rawSize, 4);
    std::memcpy(&img[sec + 20], &rawOff, 4);

    auto info = pe::Parse(img);
    REQUIRE(info);
    CHECK(info->machine == 0x14C);
    CHECK(info->entryRva == 0x31DF13);
    CHECK(pe::VaToFileOffset(*info, 0x401010) == std::optional<std::uint32_t>(0x410));
    CHECK_FALSE(pe::VaToFileOffset(*info, 0x402800));  // inside the section, but beyond raw data
    CHECK_FALSE(pe::VaToFileOffset(*info, 0x300000));

    std::vector<std::uint8_t> text = {'x', 'U', 0, 'l', 0, 't', 0, 'i', 0, 'm', 0, 'a', 0, 't', 0, 'e', 0};
    CHECK(pe::ContainsText(text, "ultimate"));
    CHECK_FALSE(pe::ContainsText(text, "loader"));
}

TEST_CASE("vorbisFile.dll classification") {
    const KnownBinary& silent = gen::kSilentLoaders[0];
    CHECK(ClassifyVorbisFile(silent.size, silent.crc, true) == LoaderKind::Silent);
    CHECK(ClassifyVorbisFile(gen::kOriginalVorbisFile.size, gen::kOriginalVorbisFile.crc, false) == LoaderKind::None);
    CHECK(ClassifyVorbisFile(1392864, 0xbc38c19d, false) == LoaderKind::Other);  // Arizona's loader
    CHECK(ClassifyVorbisFile(60000, 0x1234, true) == LoaderKind::Silent);         // other Silent build
}

TEST_CASE("Arizona settings.json") {
    fs::path launcher = L"C:\\Users\\u\\AppData\\Local\\Programs\\Arizona Games Launcher";
    auto games = ParseArizonaSettings(
        R"({"arizona":{"options":[{"id":"autoClean","value":true}],"gamePath":"D:\\Games\\Arizona"},"rodina":{"options":[]}})", launcher);
    REQUIRE(games.size() == std::size(kArizonaGameIds));
    CHECK(games[0].id == "arizona");
    CHECK(games[0].dir == fs::path(L"D:\\Games\\Arizona"));
    CHECK(games[0].customPath);
    CHECK(games[0].autoClean);
    CHECK(games[1].id == "rodina");
    CHECK(games[1].dir == launcher / L"bin" / L"rodina");
    CHECK_FALSE(games[1].autoClean);
    CHECK(games[0].title == "Arizona RP");
    CHECK(ArizonaTitle("rodina_staging") == "Rodina RP (тестовый сервер)");

    auto broken = ParseArizonaSettings("{not json", launcher);
    REQUIRE(broken.size() == std::size(kArizonaGameIds));
    CHECK(broken[0].dir == launcher / L"bin" / L"arizona");

    CHECK(ParseArizonaSettings("", fs::path()).empty());

    CHECK(IsArizonaManagedFile(L"cleo.asi"));
    CHECK(IsArizonaManagedFile(L"CLEO.asi"));
    CHECK(IsArizonaManagedFile(L"cleo\\IniFiles.cleo"));
    CHECK(IsArizonaManagedFile(L"vorbisFile.dll"));
    CHECK_FALSE(IsArizonaManagedFile(L"MoonLoader.asi"));
    CHECK_FALSE(IsArizonaManagedFile(L"moonloader\\lib\\imgui.lua"));
}

TEST_CASE("command line quoting round-trips through CommandLineToArgvW") {
    for (std::wstring arg : {L"C:\\Games\\GTA", L"C:\\My Games\\GTA San Andreas\\", L"with \"quote\"", L"", L"Игры\\ГТА 140\\"}) {
        std::wstring cmd = L"app.exe " + QuoteArg(arg) + L" tail";
        int n = 0;
        LPWSTR* argv = CommandLineToArgvW(cmd.c_str(), &n);
        REQUIRE(n == 3);
        CHECK(std::wstring(argv[1]) == arg);
        CHECK(std::wstring(argv[2]) == L"tail");
        LocalFree(argv);
    }
}

TEST_CASE("elevated relaunch parameters survive CommandLineToArgvW + ParseArgs") {
    Args base;
    base.fontsDir = fs::path(LR"(C:\Temp\my fonts\)");
    base.simulateMissingFonts = true;
    base.throttleMs = 5;
    Options o;
    o.Set(ItemId::DirectX, false);
    std::wstring params = BuildElevatedParameters(base, LR"(C:\Игры\GTA San Andreas\)", EncodeOptions(o), 0x0123456789abcdefull,
                                                  true, std::pair{-1200, 40});
    std::wstring cmd = L"uf-installer.exe " + params;
    int n = 0;
    LPWSTR* argv = CommandLineToArgvW(cmd.c_str(), &n);
    Args a = ParseArgs(n, argv);
    LocalFree(argv);
    CHECK(a.elevated);
    CHECK(a.install);
    REQUIRE(a.target);
    CHECK(*a.target == fs::path(LR"(C:\Игры\GTA San Andreas\)"));
    CHECK(a.planHash == std::optional<std::uint64_t>(0x0123456789abcdefull));
    CHECK(DecodeOptions(a.opts).toggles[static_cast<std::size_t>(ItemId::DirectX)] == std::optional<bool>(false));
    REQUIRE(a.fontsDir);
    CHECK(*a.fontsDir == *base.fontsDir);
    CHECK(a.simulateMissingFonts);
    CHECK(a.throttleMs == 5);
    CHECK(a.pos == std::optional<std::pair<int, int>>(std::pair{-1200, 40}));
    CHECK(a.unknown.empty());
}

TEST_CASE("options encoding") {
    Options o;
    o.Set(ItemId::Script, false);
    o.Set(ItemId::Fonts, true);
    o.overwriteLibs = true;
    Options d = DecodeOptions(EncodeOptions(o));
    CHECK(d.toggles[static_cast<std::size_t>(ItemId::Script)] == std::optional<bool>(false));
    CHECK(d.toggles[static_cast<std::size_t>(ItemId::Fonts)] == std::optional<bool>(true));
    CHECK_FALSE(d.toggles[static_cast<std::size_t>(ItemId::Cleo)].has_value());
    CHECK(d.overwriteLibs);
    CHECK_FALSE(d.customFontsDir);
}
