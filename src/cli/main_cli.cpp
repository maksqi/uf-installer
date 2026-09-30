// uf-cli: console front-end over the same core as the GUI. Used for tests and automation.
//
//   uf-cli scan [--no-scan] [--json]
//   uf-cli plan <game dir> [--json] [--opts ...] [--fonts-dir <dir>] [--simulate-missing-fonts] [--simulate-missing-d3dx9]
//   uf-cli install <game dir> --yes [--no-elevate] [--opts ...] [--fonts-dir <dir>] [--fail-after N] [--throttle-ms N]
//   uf-cli payload
//   uf-cli dxcheck      (downloads dxwebsetup.exe to %TEMP% and checks the Microsoft signature)
#include <windows.h>

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdio>
#include <format>
#include <iostream>
#include <thread>

#include <urlmon.h>

#include "core/args.h"
#include "core/directx.h"
#include "core/fsutil.h"
#include "core/discovery.h"
#include "core/elevation.h"
#include "core/fonts.h"
#include "core/installer.h"
#include "core/arizona.h"
#include "core/inspect.h"
#include "core/log.h"
#include "core/plan.h"
#include "payload_manifest.gen.h"

using namespace uf;
using nlohmann::json;

namespace {

void Print(const std::string& s) {
    std::fwrite(s.data(), 1, s.size(), stdout);
    std::fputc('\n', stdout);
}

const char* StateName(ItemState s) {
    switch (s) {
        case ItemState::Ok: return "ok";
        case ItemState::Install: return "install";
        case ItemState::Update: return "update";
        case ItemState::Repair: return "repair";
        case ItemState::Skipped: return "skipped";
        case ItemState::Managed: return "managed";
        case ItemState::Warning: return "warning";
        case ItemState::Error: return "error";
    }
    return "?";
}

const char* SeverityName(Severity s) { return s == Severity::Info ? "info" : s == Severity::Warning ? "warning" : "error"; }

json ReportJson(const FolderReport& r) {
    json j;
    j["dir"] = PathUtf8(r.dir);
    j["gta"] = std::string(GtaVersionName(r.gta));
    j["samp"] = r.hasSamp ? std::string(SampVersionName(r.samp)) : "";
    j["loader"] = LoaderName(r);
    j["cleo"] = r.cleo ? r.cleo->version : "";
    j["sampfuncs"] = r.sampfuncs ? r.sampfuncs->version + (r.sampfuncs->target.empty() ? "" : " (" + r.sampfuncs->target + ")") : "";
    j["moonloader"] = r.moonloader ? r.moonloader->version : "";
    j["bass"] = r.bassVersion;
    j["lua51"] = r.hasLua51;
    j["libUnitsMissing"] = r.MissingUnitCount();
    j["libUnitsTotal"] = r.libUnits.size();
    json scripts = json::array();
    for (const UfScript& s : r.ufScripts) scripts.push_back({{"file", PathUtf8(s.file.filename())}, {"version", s.version}, {"loadable", s.loadable}});
    j["ufScripts"] = scripts;
    j["ufExactInstalled"] = r.ufExactInstalled;
    j["d3dx9"] = r.d3dx9;
    json fonts = json::array();
    for (const FontState& f : r.fonts) fonts.push_back({{"file", f.file}, {"present", f.present}, {"required", f.required}, {"bundled", f.bundled}});
    j["fonts"] = fonts;
    j["arizona"] = r.arizona ? r.arizonaId : "";
    j["arizonaAutoClean"] = r.arizonaAutoClean;
    j["gameRunning"] = r.gameRunning;
    j["nonAsciiPath"] = r.nonAsciiPath;
    j["writable"] = r.writable;
    return j;
}

json PlanJson(const InstallPlan& p) {
    json j;
    json items = json::array();
    for (const PlanItem& it : p.items)
        items.push_back({{"title", it.title}, {"state", StateName(it.state)}, {"status", it.status}, {"toggleable", it.toggleable},
                         {"enabled", it.enabled}, {"admin", it.admin}});
    j["items"] = items;
    json notices = json::array();
    for (const Notice& n : p.notices) notices.push_back({{"severity", SeverityName(n.severity)}, {"text", n.text}});
    j["notices"] = notices;
    json files = json::array();
    for (const FileOp& f : p.files)
        files.push_back({{"op", f.kind == OpKind::Copy ? (f.mode == CopyMode::Replace ? "replace" : "add") : "move-to-backup"},
                         {"path", PathUtf8(f.rel)}});
    j["files"] = files;
    json dirs = json::array();
    for (const fs::path& d : p.dirs) dirs.push_back(PathUtf8(d));
    j["dirs"] = dirs;
    json fontOps = json::array();
    for (const FontOp& f : p.fonts) fontOps.push_back(f.file);
    j["fonts"] = fontOps;
    j["directx"] = p.directx;
    j["needsAdmin"] = p.needsAdmin;
    j["adminReasons"] = p.adminReasons;
    j["blocked"] = p.blocked;
    j["blockReason"] = p.blockReason;
    j["hash"] = std::format("{:016x}", p.Hash());
    return j;
}

void PrintPlanText(const FolderReport& r, const InstallPlan& p) {
    Print(std::format("Папка: {}{}", PathUtf8(r.dir), r.arizona ? " [Arizona: " + ArizonaTitle(r.arizonaId) + "]" : ""));
    for (const PlanItem& it : p.items)
        Print(std::format("  {:<26} {:<8} {}{}{}", it.title, StateName(it.state), it.status,
                          it.toggleable ? (it.enabled ? "  [x]" : "  [ ]") : "", it.admin ? "  (admin)" : ""));
    for (const Notice& n : p.notices) Print(std::format("  ! {}: {}", SeverityName(n.severity), n.text));
    std::size_t add = 0, replace = 0, move = 0;
    for (const FileOp& f : p.files) (f.kind == OpKind::MoveToBackup ? move : f.mode == CopyMode::Replace ? replace : add)++;
    Print(std::format("  Файлов: добавить {}, заменить {}, в бэкап {}; папок {}; шрифтов {}; DirectX {}", add, replace, move,
                      p.dirs.size(), p.fonts.size(), p.directx ? "да" : "нет"));
    if (p.needsAdmin) {
        std::string reasons;
        for (const auto& s : p.adminReasons) reasons += (reasons.empty() ? "" : ", ") + s;
        Print("  Нужны права администратора: " + reasons);
    }
    if (p.blocked) Print("  УСТАНОВКА НЕВОЗМОЖНА: " + p.blockReason);
}

InspectOptions MakeInspectOptions(const Args& a) {
    InspectOptions o;
    o.fontsDir = a.fontsDir;
    o.simulateMissingFonts = a.simulateMissingFonts;
    o.simulateMissingD3dx9 = a.simulateMissingD3dx9;
    return o;
}

int CmdScan(const Args& a) {
    DiscoveryConfig cfg;
    cfg.inspect = MakeInspectOptions(a);
    cfg.diskScan = !a.noScan;
    cfg.arizonaRoot = a.arizonaRoot;
    cfg.arizonaSettings = a.arizonaSettings;
    Discovery d(cfg);
    auto t0 = std::chrono::steady_clock::now();
    d.Start([] {});
    for (;;) {
        auto list = d.Snapshot();
        bool inspected = std::all_of(list.begin(), list.end(), [](const GameEntry& e) { return e.report != nullptr; });
        if (d.HintsDone() && !d.ScanRunning() && inspected) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    double total = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    auto list = d.Snapshot();
    if (a.json) {
        json arr = json::array();
        for (const GameEntry& e : list) {
            json j = ReportJson(*e.report);
            j["title"] = e.title;
            j["sources"] = DescribeSources(e.sources);
            arr.push_back(j);
        }
        Print(json{{"games", arr}, {"scanSeconds", d.ScanSeconds()}, {"dirsScanned", d.DirsScanned()}}.dump(2));
    } else {
        for (const GameEntry& e : list) {
            const FolderReport& r = *e.report;
            Print(std::format("{} — {}", e.title, PathUtf8(e.dir)));
            Print(std::format("    источник: {}; SA-MP {}; CLEO {}; SF {}; ML {}; UF: {}", DescribeSources(e.sources),
                              r.hasSamp ? SampVersionName(r.samp) : "нет", r.cleo ? r.cleo->version : "нет",
                              r.sampfuncs ? r.sampfuncs->version : "нет", r.moonloader ? r.moonloader->version : "нет",
                              r.ufScripts.empty() ? "нет" : r.ufScripts.front().version));
        }
        Print(std::format("Найдено: {}. Скан дисков: {:.1f} с, папок: {}. Всего {:.1f} с.", list.size(), d.ScanSeconds(), d.DirsScanned(), total));
    }
    return 0;
}

int CmdPlan(const Args& a, bool install) {
    if (a.positional.size() < 2) {
        Print("Укажите папку игры.");
        return 2;
    }
    fs::path dir = a.positional[1];
    ArizonaInfo arizona = LoadArizonaInfo(a.arizonaRoot, a.arizonaSettings);
    InspectOptions io = MakeInspectOptions(a);
    io.arizona = &arizona;
    FolderReport report = Inspect(dir, io);
    Options opts = DecodeOptions(a.opts);
    if (a.fontsDir) opts.customFontsDir = true;
    InstallPlan plan = BuildPlan(report, opts);

    if (!install) {
        if (a.json)
            Print(json{{"report", ReportJson(report)}, {"plan", PlanJson(plan)}}.dump(2));
        else
            PrintPlanText(report, plan);
        return 0;
    }

    PrintPlanText(report, plan);
    if (plan.blocked) return 4;
    if (plan.Empty()) {
        Print("Нечего устанавливать.");
        return 0;
    }
    if (!a.yes) {
        Print("Добавьте --yes, чтобы установить.");
        return 2;
    }
    if (plan.needsAdmin && !IsProcessElevated() && !a.noElevate) {
        Print("Нужны права администратора — запустите uf-cli из консоли администратора или добавьте --no-elevate.");
        return 3;
    }
    InstallEnv env;
    env.fontsDir = a.fontsDir ? *a.fontsDir : SystemFontsDir();
    env.registerFonts = !a.fontsDir;
    env.failAfter = a.failAfter;
    env.throttleMs = a.throttleMs;
    InstallResult res = RunInstall(report.dir, plan, env, nullptr, [](const std::string& s) { Print("  > " + s); });
    if (a.json)
        Print(json{{"ok", res.ok},
                   {"rolledBack", res.rolledBack},
                   {"error", res.error},
                   {"installed", res.installed},
                   {"replaced", res.replaced},
                   {"skipped", res.skipped},
                   {"movedOld", res.movedOld},
                   {"fonts", res.fontsInstalled},
                   {"backupDir", PathUtf8(res.backupDir)},
                   {"warnings", res.warnings},
                   {"seconds", res.seconds}}
                  .dump(2));
    else
        Print(res.ok ? std::format("УСПЕХ: новых {}, заменено {}, пропущено {}, старых версий {}, шрифтов {}, {:.2f} с", res.installed,
                                   res.replaced, res.skipped, res.movedOld, res.fontsInstalled, res.seconds)
                     : "ОШИБКА: " + res.error + (res.rolledBack ? " (изменения откатены)" : ""));
    return res.ok ? 0 : 1;
}

// Downloads dxwebsetup.exe and verifies its signature (does not run it).
int CmdDxCheck() {
    TempDir temp;
    fs::path exe = temp.path() / L"dxwebsetup.exe";
    HRESULT hr = URLDownloadToFileW(nullptr, kDxWebSetupUrl, exe.c_str(), 0, nullptr);
    if (FAILED(hr)) {
        Print(std::format("Скачать не удалось: 0x{:08X}", static_cast<unsigned>(hr)));
        return 1;
    }
    std::string signer;
    bool ok = VerifyMicrosoftSignature(exe, &signer);
    Print(std::format("dxwebsetup.exe: {} байт, подписант «{}», проверка подписи: {}", FileSize(exe).value_or(0), signer, ok ? "OK" : "НЕ ПРОЙДЕНА"));
    Print(std::format("d3dx9_43.dll (x86) в системе: {}", HasD3DX9_43(fs::path()) ? "есть" : "нет"));
    return ok ? 0 : 1;
}

int CmdPayload() {
    Print(std::format("UltraFuck {}, MoonLoader {}, SAMPFUNCS {} ({}), CLEO {}, BASS {}", gen::kScriptVersion, gen::kMoonLoaderVersion,
                      gen::kSampfuncsVersion, gen::kSampfuncsTarget, gen::kCleoVersion, gen::kBassVersion));
    Print(std::format("Файлов: {}, папок: {}, шрифтов: {}", std::size(gen::kFiles), std::size(gen::kDirs), std::size(gen::kFonts)));
    return 0;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    SetConsoleOutputCP(CP_UTF8);
    Args a = ParseArgs(argc, argv);
    log::Init(GetFolder(Folder::Temp) / L"uf-installer.log");
    log::Info("uf-cli started");
    if (a.positional.empty()) {
        Print("uf-cli scan | plan <dir> | install <dir> --yes | payload   (см. src/cli/main_cli.cpp)");
        return 2;
    }
    std::wstring cmd = a.positional[0];
    if (cmd == L"scan") return CmdScan(a);
    if (cmd == L"plan") return CmdPlan(a, false);
    if (cmd == L"install") return CmdPlan(a, true);
    if (cmd == L"payload") return CmdPayload();
    if (cmd == L"dxcheck") return CmdDxCheck();
    Print("Неизвестная команда.");
    return 2;
}
