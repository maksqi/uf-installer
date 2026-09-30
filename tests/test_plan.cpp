#include <doctest/doctest.h>

#include <algorithm>
#include <set>

#include "core/i18n.h"
#include "core/plan.h"
#include "payload_manifest.gen.h"

using namespace uf;

namespace {

const fs::path kDir = L"C:\\Games\\GTA";

FolderReport BareReport() {
    FolderReport r;
    r.dir = kDir;
    r.hasGtaExe = true;
    r.gta = GtaVersion::US10;
    r.hasSamp = true;
    r.samp = SampVersion::R1;
    r.loader = LoaderKind::None;
    r.payloadFileExists.assign(std::size(gen::kFiles), 0);
    r.payloadDirExists.assign(std::size(gen::kDirs), 0);
    std::set<std::string> units;
    for (const PayloadFile& f : gen::kFiles)
        if (f.comp == Comp::Lib) units.insert(f.unit);
    for (const std::string& u : units) r.libUnits.push_back({u, false, false});
    r.d3dx9 = true;
    for (const PayloadFont& f : gen::kFonts) r.fonts.push_back({f.file, f.regName, true, f.required, true});
    return r;
}

FolderReport FullReport() {
    FolderReport r = BareReport();
    r.loader = LoaderKind::Silent;
    r.hasHookedVorbis = true;
    r.cleo = AsiInfo{kDir / L"CLEO.asi", "4.3.22", {}};
    r.sampfuncs = AsiInfo{kDir / L"SAMPFUNCS.asi", "5.4.1-final rel.21", "0.3.7-R1"};
    r.moonloader = AsiInfo{kDir / L"MoonLoader.asi", "026.5-beta", {}};
    r.hasBass = true;
    r.hasLua51 = true;
    r.payloadFileExists.assign(std::size(gen::kFiles), 1);
    r.payloadDirExists.assign(std::size(gen::kDirs), 1);
    for (auto& u : r.libUnits) u.anyPresent = u.complete = true;
    r.ufExactInstalled = true;
    r.ufScripts.push_back({kDir / ToWide(gen::kScriptDest), gen::kScriptVersion, true});
    return r;
}

const FileOp* FindOp(const InstallPlan& p, std::wstring_view rel) {
    for (const FileOp& f : p.files)
        if (IEquals(f.rel.native(), rel)) return &f;
    return nullptr;
}

ItemState StateOf(const InstallPlan& p, ItemId id) { return p.Find(id)->state; }

bool HasNotice(const InstallPlan& p, Severity s) {
    return std::any_of(p.notices.begin(), p.notices.end(), [&](const Notice& n) { return n.severity == s; });
}

}  // namespace

TEST_CASE("bare game folder: everything is installed") {
    InstallPlan p = BuildPlan(BareReport(), {});
    CHECK(StateOf(p, ItemId::AsiLoader) == ItemState::Install);
    CHECK(StateOf(p, ItemId::Cleo) == ItemState::Install);
    CHECK(StateOf(p, ItemId::Sampfuncs) == ItemState::Install);
    CHECK(StateOf(p, ItemId::MoonLoader) == ItemState::Install);
    CHECK(StateOf(p, ItemId::Libs) == ItemState::Install);
    CHECK(StateOf(p, ItemId::Script) == ItemState::Install);
    CHECK(StateOf(p, ItemId::Fonts) == ItemState::Ok);
    CHECK(StateOf(p, ItemId::DirectX) == ItemState::Ok);

    const FileOp* vorbis = FindOp(p, L"vorbisFile.dll");
    REQUIRE(vorbis);
    CHECK(vorbis->mode == CopyMode::Replace);
    CHECK(FindOp(p, L"vorbisHooked.dll"));
    CHECK(FindOp(p, L"CLEO.asi"));
    CHECK(FindOp(p, L"SAMPFUNCS.asi"));
    CHECK(FindOp(p, L"MoonLoader.asi"));
    CHECK(FindOp(p, L"lua51.dll"));
    CHECK(FindOp(p, L"bass.dll"));
    CHECK(FindOp(p, L"moonloader\\lib\\samp\\events.lua"));
    CHECK(FindOp(p, ToWide(gen::kScriptDest)));
    CHECK(FindOp(p, L"moonloader\\config\\UltraFuck\\Settings.ini")->mode == CopyMode::AddIfMissing);
    CHECK(std::count(p.dirs.begin(), p.dirs.end(), fs::path(L"cleo\\cleo_saves")) == 1);
    CHECK_FALSE(p.needsAdmin);
    CHECK_FALSE(p.blocked);
}

TEST_CASE("full R1 folder with the same script: nothing to do") {
    InstallPlan p = BuildPlan(FullReport(), {});
    CHECK(p.Empty());
    CHECK(StateOf(p, ItemId::Script) == ItemState::Ok);
    CHECK_FALSE(HasNotice(p, Severity::Error));
}

TEST_CASE("older/newer script versions are replaced by the bundled one") {
    FolderReport r = FullReport();
    r.ufExactInstalled = false;
    r.ufScripts = {{kDir / L"moonloader" / L"UltraFuck 2.4.lua", "2.4", true}};
    InstallPlan p = BuildPlan(r, {});
    CHECK(StateOf(p, ItemId::Script) == ItemState::Update);
    const FileOp* move = FindOp(p, L"moonloader\\UltraFuck 2.4.lua");
    REQUIRE(move);
    CHECK(move->kind == OpKind::MoveToBackup);
    CHECK(FindOp(p, ToWide(gen::kScriptDest))->mode == CopyMode::Replace);
    // user keeps the current version
    Options keep;
    keep.Set(ItemId::Script, false);
    InstallPlan kept = BuildPlan(r, keep);
    CHECK(StateOf(kept, ItemId::Script) == ItemState::Skipped);
    CHECK(kept.Empty());
}

TEST_CASE(".aluac copies are reported but never touched") {
    FolderReport r = FullReport();
    r.ufScripts.push_back({kDir / L"moonloader" / L"UltraFuck_2.4-fix.aluac", "2.4", false});
    InstallPlan p = BuildPlan(r, {});
    CHECK(p.Empty());
    CHECK(HasNotice(p, Severity::Info));
}

TEST_CASE("reinstall checkbox rewrites the script") {
    Options o;
    o.Set(ItemId::Script, true);
    InstallPlan p = BuildPlan(FullReport(), o);
    CHECK(StateOf(p, ItemId::Script) == ItemState::Update);
    CHECK(FindOp(p, ToWide(gen::kScriptDest)));
}

TEST_CASE("Arizona folder: launcher-managed files are never written") {
    FolderReport r = BareReport();
    r.arizona = true;
    r.arizonaId = "arizona";
    r.samp = SampVersion::R3_1;
    r.loader = LoaderKind::Other;
    r.cleo = AsiInfo{kDir / L"cleo.asi", "4.4.0", {}};
    r.sampfuncs = AsiInfo{kDir / L"SAMPFUNCS.asi", "5.5.0 rel.22", "0.3.7 R3-1"};
    r.moonloader = AsiInfo{kDir / L"MoonLoader.asi", "026.5-beta", {}};
    r.hasBass = true;
    r.hasLua51 = true;
    r.ufScripts = {{kDir / L"moonloader" / L"UltraFuck 2.4.lua", "2.4", true}};
    InstallPlan p = BuildPlan(r, {});
    CHECK(StateOf(p, ItemId::AsiLoader) == ItemState::Managed);
    CHECK(StateOf(p, ItemId::Sampfuncs) == ItemState::Ok);
    CHECK(StateOf(p, ItemId::Script) == ItemState::Update);
    for (const FileOp& f : p.files) CHECK_FALSE(IsArizonaManagedFile(f.rel));
    CHECK(FindOp(p, L"moonloader\\lib\\imgui.lua"));

    // missing CLEO/bass in Arizona: point to the launcher, do not install ours
    r.cleo.reset();
    r.hasBass = false;
    InstallPlan q = BuildPlan(r, {});
    CHECK(StateOf(q, ItemId::Cleo) == ItemState::Error);
    CHECK_FALSE(FindOp(q, L"bass.dll"));
    CHECK_FALSE(FindOp(q, L"CLEO.asi"));
}

TEST_CASE("SAMPFUNCS 5.4.1 is not installed on R3") {
    FolderReport r = BareReport();
    r.samp = SampVersion::R3_1;
    InstallPlan p = BuildPlan(r, {});
    CHECK(StateOf(p, ItemId::Sampfuncs) == ItemState::Error);
    CHECK_FALSE(FindOp(p, L"SAMPFUNCS.asi"));
    CHECK(HasNotice(p, Severity::Error));
}

TEST_CASE("SAMPFUNCS built for another SA-MP version is a warning") {
    FolderReport r = FullReport();
    r.samp = SampVersion::R3_1;
    InstallPlan p = BuildPlan(r, {});
    CHECK(StateOf(p, ItemId::Sampfuncs) == ItemState::Warning);
    CHECK(HasNotice(p, Severity::Warning));
}

TEST_CASE("missing fonts and d3dx9 need admin rights") {
    FolderReport r = FullReport();
    for (FontState& f : r.fonts) f.present = false;
    r.d3dx9 = false;
    InstallPlan p = BuildPlan(r, {});
    CHECK(StateOf(p, ItemId::Fonts) == ItemState::Install);
    CHECK(p.fonts.size() == std::size(gen::kFonts));
    CHECK(p.directx);
    CHECK(p.needsAdmin);
    CHECK(p.adminReasons.size() == 2);

    Options o;
    o.customFontsDir = true;
    o.Set(ItemId::DirectX, false);
    InstallPlan q = BuildPlan(r, o);
    CHECK_FALSE(q.needsAdmin);
    CHECK_FALSE(q.directx);
    CHECK(HasNotice(q, Severity::Error));  // d3dx9 is required
}

TEST_CASE("non-writable folder needs admin") {
    FolderReport r = BareReport();
    r.writable = false;
    InstallPlan p = BuildPlan(r, {});
    CHECK(p.needsAdmin);
    CHECK(p.Find(ItemId::Cleo)->admin);
}

TEST_CASE("only missing library units are added") {
    FolderReport r = FullReport();
    std::string missingUnit;
    for (auto& u : r.libUnits)
        if (u.unit == "requests") {
            u.anyPresent = false;
            missingUnit = u.unit;
        }
    REQUIRE_FALSE(missingUnit.empty());
    for (std::size_t i = 0; i < std::size(gen::kFiles); ++i)
        if (gen::kFiles[i].comp == Comp::Lib && gen::kFiles[i].unit == missingUnit) r.payloadFileExists[i] = 0;
    InstallPlan p = BuildPlan(r, {});
    CHECK(StateOf(p, ItemId::Libs) == ItemState::Install);
    REQUIRE(p.files.size() >= 1);
    for (const FileOp& f : p.files) CHECK(f.rel.native().find(L"moonloader\\lib\\requests") == 0);

    Options o;
    o.overwriteLibs = true;
    InstallPlan all = BuildPlan(FullReport(), o);
    CHECK(StateOf(all, ItemId::Libs) == ItemState::Update);
    CHECK(std::all_of(all.files.begin(), all.files.end(), [](const FileOp& f) { return f.mode == CopyMode::Replace; }));
}

TEST_CASE("overwriting libraries writes only the files that differ from the bundle") {
    Options o;
    o.overwriteLibs = true;

    FolderReport same = FullReport();
    same.payloadFileSame.assign(std::size(gen::kFiles), 1);
    InstallPlan none = BuildPlan(same, o);
    CHECK(StateOf(none, ItemId::Libs) == ItemState::Ok);
    CHECK(none.Empty());

    FolderReport changed = same;
    std::size_t changedIndex = std::size(gen::kFiles);
    for (std::size_t i = 0; i < std::size(gen::kFiles) && changedIndex == std::size(gen::kFiles); ++i)
        if (gen::kFiles[i].comp == Comp::Lib) changedIndex = i;
    REQUIRE(changedIndex < std::size(gen::kFiles));
    changed.payloadFileSame[changedIndex] = 0;
    InstallPlan one = BuildPlan(changed, o);
    CHECK(StateOf(one, ItemId::Libs) == ItemState::Update);
    REQUIRE(one.files.size() == 1);
    CHECK(one.files[0].rel == fs::path(ToWide(gen::kFiles[changedIndex].dest)));
    CHECK(one.files[0].mode == CopyMode::Replace);
}

TEST_CASE("plan texts follow the interface language") {
    SetLang(Lang::En);
    InstallPlan en = BuildPlan(BareReport(), {});
    SetLang(Lang::Uk);
    InstallPlan uk = BuildPlan(BareReport(), {});
    SetLang(Lang::Ru);
    InstallPlan ru = BuildPlan(BareReport(), {});
    CHECK(en.Find(ItemId::Cleo)->status == "not installed");
    CHECK(uk.Find(ItemId::Cleo)->status == "не встановлено");
    CHECK(uk.Find(ItemId::Libs)->title == "Бібліотеки MoonLoader");
    CHECK(ru.Find(ItemId::Cleo)->status == "не установлен");
    CHECK(en.Hash() == ru.Hash());
    CHECK(uk.Hash() == ru.Hash());
}

TEST_CASE("Silent loader without vorbisHooked.dll is repaired") {
    FolderReport r = FullReport();
    r.loader = LoaderKind::SilentNoHooked;
    for (std::size_t i = 0; i < std::size(gen::kFiles); ++i)
        if (IEquals(ToWide(gen::kFiles[i].dest), L"vorbisHooked.dll")) r.payloadFileExists[i] = 0;
    InstallPlan p = BuildPlan(r, {});
    CHECK(StateOf(p, ItemId::AsiLoader) == ItemState::Repair);
    REQUIRE(p.files.size() == 1);
    CHECK(p.files[0].rel == fs::path(L"vorbisHooked.dll"));
}

TEST_CASE("running game blocks the installation") {
    FolderReport r = BareReport();
    r.gameRunning = true;
    InstallPlan p = BuildPlan(r, {});
    CHECK(p.blocked);
    CHECK_FALSE(p.blockReason.empty());
}

TEST_CASE("plan hash depends on the operations") {
    InstallPlan a = BuildPlan(BareReport(), {});
    InstallPlan b = BuildPlan(BareReport(), {});
    CHECK(a.Hash() == b.Hash());
    Options o;
    o.Set(ItemId::Cleo, false);
    CHECK(BuildPlan(BareReport(), o).Hash() != a.Hash());
}
