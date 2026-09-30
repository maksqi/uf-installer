#include "core/plan.h"

#include <algorithm>
#include <charconv>
#include <format>
#include <functional>
#include <set>

#include "core/arizona.h"
#include "core/i18n.h"
#include "core/versions.h"
#include "payload_manifest.gen.h"

namespace uf {

namespace {

std::string ShortVersion(std::string v) {
    while (v.size() > 2 && v.ends_with(".0") && std::count(v.begin(), v.end(), '.') > 2) v.resize(v.size() - 2);
    return v;
}

std::string Join(const std::vector<std::string>& parts, std::size_t limit = 12) {
    std::string out;
    for (std::size_t i = 0; i < parts.size() && i < limit; ++i) {
        if (i) out += ", ";
        out += parts[i];
    }
    if (parts.size() > limit) out += F(" и ещё {}", " and {} more", parts.size() - limit);
    return out;
}

class Builder {
public:
    Builder(const FolderReport& r, const Options& o) : r_(r), o_(o) {}

    InstallPlan plan;

    bool Toggle(ItemId id, bool def) const {
        const auto& t = o_.toggles[static_cast<std::size_t>(id)];
        return t.has_value() ? *t : def;
    }

    PlanItem& Add(ItemId id, std::string title) {
        PlanItem item;
        item.id = id;
        item.title = std::move(title);
        plan.items.push_back(std::move(item));
        return plan.items.back();
    }

    // Makes the item optional (checkbox) and returns whether the action is enabled.
    bool Optional(PlanItem& it, bool def, ItemState onState) {
        it.toggleable = true;
        it.enabled = Toggle(it.id, def);
        it.state = it.enabled ? onState : ItemState::Skipped;
        return it.enabled;
    }

    void Note(Severity s, std::string text) { plan.notices.push_back({s, std::move(text)}); }

    bool Exists(std::size_t i) const { return i < r_.payloadFileExists.size() && r_.payloadFileExists[i]; }
    // The file in the game folder is byte-identical to the bundled one (checked for libraries only).
    bool Same(std::size_t i) const { return i < r_.payloadFileSame.size() && r_.payloadFileSame[i]; }

    void Copy(ItemId item, Comp comp, CopyMode mode, const std::function<bool(const PayloadFile&)>& filter = {}) {
        for (std::size_t i = 0; i < std::size(gen::kFiles); ++i) {
            const PayloadFile& f = gen::kFiles[i];
            if (f.comp != comp || (filter && !filter(f))) continue;
            fs::path rel = ToWide(f.dest);
            if (r_.arizona && IsArizonaManagedFile(rel)) continue;
            if (mode == CopyMode::AddIfMissing && Exists(i)) continue;
            if (mode == CopyMode::Replace && Same(i)) continue;
            plan.files.push_back({OpKind::Copy, item, f.entry, rel, mode, f.size, f.crc});
        }
    }

    void Dirs(Comp comp) {
        for (std::size_t i = 0; i < std::size(gen::kDirs); ++i) {
            bool exists = i < r_.payloadDirExists.size() && r_.payloadDirExists[i];
            if (gen::kDirs[i].comp == comp && !exists) plan.dirs.emplace_back(ToWide(gen::kDirs[i].dest));
        }
    }

    bool AnyMissing(Comp comp) const {
        for (std::size_t i = 0; i < std::size(gen::kFiles); ++i)
            if (gen::kFiles[i].comp == comp && !Exists(i)) return true;
        return false;
    }

private:
    const FolderReport& r_;
    const Options& o_;
};

bool DestIs(const PayloadFile& f, std::wstring_view name) { return IEquals(ToWide(f.dest), name); }

}  // namespace

InstallPlan BuildPlan(const FolderReport& r, const Options& o) {
    Builder b(r, o);
    InstallPlan& p = b.plan;
    const bool az = r.arizona;
    const char* launcherCheck = T("запустите в нём проверку файлов игры.", "run the game file check in it.");

    // ---- GTA SA
    {
        PlanItem& it = b.Add(ItemId::Gta, "GTA San Andreas");
        if (!r.hasGtaExe) {
            it.state = ItemState::Error;
            it.status = T("gta_sa.exe не найден", "gta_sa.exe not found");
            p.blocked = true;
            p.blockReason = T("В выбранной папке нет gta_sa.exe.", "The selected folder has no gta_sa.exe.");
        } else if (r.gta == GtaVersion::US10) {
            it.status = T("версия 1.0 US", "version 1.0 US");
        } else {
            it.state = ItemState::Warning;
            it.status = F("версия {}", "version {}", GtaVersionName(r.gta));
            it.detail = T("MoonLoader, CLEO и SAMPFUNCS работают только с gta_sa.exe версии 1.0 US.",
                          "MoonLoader, CLEO and SAMPFUNCS only work with gta_sa.exe version 1.0 US.");
            b.Note(Severity::Warning, T("gta_sa.exe не версии 1.0 US — MoonLoader и SAMPFUNCS могут не запуститься. "
                                        "Нужен gta_sa.exe 1.0 US (даунгрейд).",
                                        "gta_sa.exe is not version 1.0 US, so MoonLoader and SAMPFUNCS may fail to start. "
                                        "You need gta_sa.exe 1.0 US (downgrade)."));
        }
    }

    // ---- SA-MP (info only)
    {
        PlanItem& it = b.Add(ItemId::Samp, "SA-MP");
        if (!r.hasSamp) {
            it.state = ItemState::Error;
            it.status = T("не установлен", "not installed");
            it.detail = T("samp.dll не найден.", "samp.dll not found.");
            b.Note(Severity::Error, T("SA-MP не найден. Установите клиент SA-MP 0.3.7 — без него скрипт работать не будет.",
                                      "SA-MP not found. Install the SA-MP 0.3.7 client: the script does not work without it."));
        } else if (r.samp == SampVersion::Unknown) {
            it.state = ItemState::Warning;
            it.status = F("неизвестная сборка {}", "unknown build {}", r.sampFileVersion);
        } else {
            it.status = std::string(SampVersionName(r.samp));
        }
    }

    // ---- ASI loader
    {
        PlanItem& it = b.Add(ItemId::AsiLoader, "ASI Loader");
        switch (r.loader) {
            case LoaderKind::Silent:
            case LoaderKind::Ual:
                it.status = LoaderName(r);
                break;
            case LoaderKind::Other:
                it.status = az ? T("загрузчик Arizona", "Arizona loader") : LoaderName(r);
                if (az) it.state = ItemState::Managed;
                break;
            case LoaderKind::SilentNoHooked:
                it.status = T("нет vorbisHooked.dll", "no vorbisHooked.dll");
                if (az) {
                    it.state = ItemState::Managed;
                    break;
                }
                it.detail = T("Будет добавлен оригинальный vorbisHooked.dll — без него в игре нет звука.",
                              "The original vorbisHooked.dll will be added: without it the game has no sound.");
                if (b.Optional(it, true, ItemState::Repair))
                    b.Copy(it.id, Comp::AsiLoader, CopyMode::AddIfMissing, [](const PayloadFile& f) { return DestIs(f, L"vorbisHooked.dll"); });
                break;
            case LoaderKind::None:
            case LoaderKind::Missing:
                it.status = LoaderName(r);
                if (az) {
                    it.state = ItemState::Error;
                    it.detail = T("Загрузчиком управляет Arizona Launcher — ", "The loader is managed by Arizona Launcher: ") +
                                std::string(launcherCheck);
                    b.Note(Severity::Error, T("Нет загрузчика ASI. Запустите проверку файлов игры в Arizona Launcher.",
                                              "No ASI loader. Run the game file check in Arizona Launcher."));
                    break;
                }
                it.detail = T("Silent's ASI Loader 1.3 — без него игра не загружает CLEO, SAMPFUNCS и MoonLoader. "
                              "Оригинальный vorbisFile.dll уйдёт в резервную копию.",
                              "Silent's ASI Loader 1.3: without it the game does not load CLEO, SAMPFUNCS and MoonLoader. "
                              "The original vorbisFile.dll goes to the backup.");
                if (b.Optional(it, true, ItemState::Install)) {
                    b.Copy(it.id, Comp::AsiLoader, CopyMode::Replace, [](const PayloadFile& f) { return DestIs(f, L"vorbisFile.dll"); });
                    b.Copy(it.id, Comp::AsiLoader, CopyMode::AddIfMissing, [](const PayloadFile& f) { return !DestIs(f, L"vorbisFile.dll"); });
                } else {
                    b.Note(Severity::Warning, T("Без ASI Loader игра не загрузит CLEO, SAMPFUNCS и MoonLoader.",
                                                "Without an ASI loader the game will not load CLEO, SAMPFUNCS and MoonLoader."));
                }
                break;
        }
    }

    // ---- CLEO
    bool cleoAfter = r.cleo.has_value();
    {
        PlanItem& it = b.Add(ItemId::Cleo, "CLEO");
        if (r.cleo) {
            it.status = r.cleo->version.empty() ? T("установлен", "installed") : r.cleo->version;
        } else if (az) {
            it.state = ItemState::Error;
            it.status = T("не найден", "not found");
            it.detail = T("CLEO ставит Arizona Launcher — ", "CLEO is installed by Arizona Launcher: ") + std::string(launcherCheck);
            b.Note(Severity::Error, T("CLEO не найден. Запустите проверку файлов игры в Arizona Launcher.",
                                      "CLEO not found. Run the game file check in Arizona Launcher."));
        } else {
            it.status = T("не установлен", "not installed");
            it.detail = F("CLEO {} — нужен для SAMPFUNCS.", "CLEO {}: required by SAMPFUNCS.", ShortVersion(gen::kCleoVersion));
            if (b.Optional(it, true, ItemState::Install)) {
                b.Copy(it.id, Comp::Cleo, CopyMode::AddIfMissing);
                b.Dirs(Comp::Cleo);
                cleoAfter = true;
            }
        }
    }

    // ---- SAMPFUNCS
    {
        PlanItem& it = b.Add(ItemId::Sampfuncs, "SAMPFUNCS");
        if (r.sampfuncs) {
            it.status = r.sampfuncs->version.empty() ? T("установлен", "installed") : r.sampfuncs->version;
            auto targets = SampVersionsFromText(r.sampfuncs->target);
            if (r.hasSamp && r.samp != SampVersion::Unknown && !targets.empty() &&
                std::find(targets.begin(), targets.end(), r.samp) == targets.end()) {
                it.state = ItemState::Warning;
                it.detail = F("Эта сборка SAMPFUNCS для SA-MP {}, а у вас {}.", "This SAMPFUNCS build is for SA-MP {}, but you have {}.",
                              r.sampfuncs->target, SampVersionName(r.samp));
                b.Note(Severity::Warning, F("SAMPFUNCS рассчитан на SA-MP {}, а установлен {} — скрипт может не работать.",
                                            "SAMPFUNCS is made for SA-MP {}, but {} is installed: the script may not work.",
                                            r.sampfuncs->target, SampVersionName(r.samp)));
            }
        } else if (az) {
            it.state = ItemState::Error;
            it.status = T("не найден", "not found");
            it.detail = T("SAMPFUNCS ставит Arizona Launcher — ", "SAMPFUNCS is installed by Arizona Launcher: ") + std::string(launcherCheck);
            b.Note(Severity::Error, T("SAMPFUNCS не найден. Запустите проверку файлов игры в Arizona Launcher.",
                                      "SAMPFUNCS not found. Run the game file check in Arizona Launcher."));
        } else if (!r.hasSamp) {
            it.state = ItemState::Error;
            it.status = T("не установлен", "not installed");
            it.detail = F("Сначала установите SA-MP {} — SAMPFUNCS {} работает только с ним.",
                          "Install SA-MP {} first: SAMPFUNCS {} only works with it.", gen::kSampfuncsTarget,
                          ShortVersion(gen::kSampfuncsVersion));
        } else if (r.samp == SampVersion::R1) {
            it.status = T("не установлен", "not installed");
            it.detail = F("SAMPFUNCS {} для SA-MP {}.", "SAMPFUNCS {} for SA-MP {}.", gen::kSampfuncsVersion, gen::kSampfuncsTarget);
            if (b.Optional(it, true, ItemState::Install)) {
                b.Copy(it.id, Comp::Sampfuncs, CopyMode::AddIfMissing);
                b.Dirs(Comp::Sampfuncs);
                if (!cleoAfter) b.Note(Severity::Warning, T("SAMPFUNCS не загрузится без CLEO.", "SAMPFUNCS will not load without CLEO."));
            } else {
                b.Note(Severity::Error, T("Без SAMPFUNCS скрипт работать не будет.", "The script does not work without SAMPFUNCS."));
            }
        } else {
            it.state = ItemState::Error;
            it.status = T("не установлен", "not installed");
            it.detail = F("В комплекте SAMPFUNCS {} — он работает только с SA-MP {}, а у вас {}.",
                          "The bundled SAMPFUNCS {} only works with SA-MP {}, but you have {}.", gen::kSampfuncsVersion,
                          gen::kSampfuncsTarget, SampVersionName(r.samp));
            b.Note(Severity::Error, F("SAMPFUNCS не установлен, а версия из комплекта подходит только к SA-MP {} (у вас {}). "
                                      "Без SAMPFUNCS для вашей версии SA-MP скрипт работать не будет.",
                                      "SAMPFUNCS is not installed, and the bundled version only fits SA-MP {} (you have {}). "
                                      "The script does not work without SAMPFUNCS for your SA-MP version.",
                                      gen::kSampfuncsTarget, SampVersionName(r.samp)));
        }
    }

    // ---- MoonLoader
    bool mlAfter = r.moonloader.has_value();
    bool mlFresh = false;
    {
        PlanItem& it = b.Add(ItemId::MoonLoader, "MoonLoader");
        if (r.moonloader) {
            it.status = r.moonloader->version.empty() ? T("установлен", "installed") : r.moonloader->version;
            int major = MoonLoaderMajor(r.moonloader->version);
            if (major >= 0 && major < 26) {
                it.detail = F("Устаревшая версия — скрипту нужен MoonLoader 026+. Будет установлен {}.",
                              "Outdated version: the script needs MoonLoader 026+. {} will be installed.", gen::kMoonLoaderVersion);
                if (b.Optional(it, true, ItemState::Update))
                    b.Copy(it.id, Comp::MoonLoader, CopyMode::Replace);
                else
                    it.state = ItemState::Warning;
            } else if (!r.hasLua51) {
                it.status = T("нет lua51.dll", "no lua51.dll");
                it.detail = T("Будет добавлен lua51.dll — без него MoonLoader не запустится.",
                              "lua51.dll will be added: MoonLoader does not start without it.");
                if (b.Optional(it, true, ItemState::Repair))
                    b.Copy(it.id, Comp::MoonLoader, CopyMode::AddIfMissing, [](const PayloadFile& f) { return DestIs(f, L"lua51.dll"); });
            }
        } else {
            it.status = T("не установлен", "not installed");
            it.detail = F("MoonLoader {} с lua51.dll и стандартными скриптами.", "MoonLoader {} with lua51.dll and the standard scripts.",
                          gen::kMoonLoaderVersion);
            if (b.Optional(it, true, ItemState::Install)) {
                b.Copy(it.id, Comp::MoonLoader, CopyMode::AddIfMissing);
                b.Copy(it.id, Comp::MoonLoaderScripts, CopyMode::AddIfMissing);
                mlAfter = mlFresh = true;
            } else {
                b.Note(Severity::Error, T("Без MoonLoader скрипт работать не будет.", "The script does not work without MoonLoader."));
            }
        }
    }

    // ---- bass.dll: imported by CLEO.asi and MoonLoader.asi
    if (!r.hasBass && !az && (cleoAfter || mlAfter)) {
        ItemId owner = mlFresh ? ItemId::MoonLoader : ItemId::Cleo;
        std::size_t before = p.files.size();
        b.Copy(owner, Comp::Bass, CopyMode::AddIfMissing);
        if (p.files.size() > before)
            b.Note(Severity::Info, T("Будет добавлен bass.dll — он нужен CLEO и MoonLoader.", "bass.dll will be added: CLEO and MoonLoader need it."));
    }

    // ---- Libraries
    {
        PlanItem& it = b.Add(ItemId::Libs, T("Библиотеки MoonLoader", "MoonLoader libraries"));
        std::set<std::string> missing;
        std::vector<std::string> names;
        for (const LibUnitState& u : r.libUnits)
            if (!u.anyPresent) {
                missing.insert(u.unit);
                names.push_back(u.unit);
            }
        if (o.overwriteLibs) {
            // Everything from the bundle: missing files are added, files that differ from it are replaced.
            std::size_t changed = 0;
            for (std::size_t i = 0; i < std::size(gen::kFiles); ++i)
                if (gen::kFiles[i].comp == Comp::Lib && !b.Same(i)) ++changed;
            if (changed == 0) {
                it.status = T("все актуальны", "all up to date");
            } else {
                it.status = F("обновить файлов: {}", "files to update: {}", changed);
                it.detail = T("Библиотеки, которые отличаются от комплекта, будут заменены, старые файлы сохранятся в резервной копии.",
                              "Libraries that differ from the bundled ones will be replaced; the old files are kept in the backup.");
                if (!missing.empty()) it.detail += F(" Будут добавлены: {}.", " To be added: {}.", Join(names));
                if (b.Optional(it, true, ItemState::Update)) b.Copy(it.id, Comp::Lib, CopyMode::Replace);
            }
        } else if (!missing.empty()) {
            it.status = F("не хватает {} из {}", "{} of {} missing", missing.size(), r.libUnits.size());
            it.detail = T("Будут добавлены: ", "To be added: ") + Join(names);
            if (b.Optional(it, true, ItemState::Install))
                b.Copy(it.id, Comp::Lib, CopyMode::AddIfMissing, [&](const PayloadFile& f) { return missing.contains(f.unit); });
            else
                b.Note(Severity::Warning, T("Без недостающих библиотек скрипт может не запуститься.",
                                            "Without the missing libraries the script may fail to start."));
        } else {
            it.status = T("все на месте", "all in place");
        }
    }

    // ---- UltraFuck script
    {
        PlanItem& it = b.Add(ItemId::Script, "UltraFuck");
        const std::string target = gen::kScriptVersion;
        std::vector<const UfScript*> same, other, inert;
        for (const UfScript& s : r.ufScripts) {
            if (!s.loadable)
                inert.push_back(&s);
            else if (!s.version.empty() && CompareDecimalVersions(s.version, target) == 0)
                same.push_back(&s);
            else
                other.push_back(&s);
        }
        auto installScript = [&] {
            b.Copy(it.id, Comp::Script, CopyMode::Replace);
            b.Copy(it.id, Comp::Config, CopyMode::AddIfMissing);
        };
        if (!other.empty()) {
            std::vector<std::string> versions, files;
            for (const UfScript* s : other) {
                versions.push_back(s->version.empty() ? PathUtf8(s->file.filename()) : s->version);
                files.push_back(PathUtf8(s->file.filename()));
            }
            it.detail = F("Старые файлы будут перенесены в резервную копию: {}. Настройки (Settings.ini) сохранятся.",
                          "Old files will be moved to the backup: {}. Your settings (Settings.ini) are kept.", Join(files));
            if (b.Optional(it, true, ItemState::Update)) {
                it.status = std::format("{} → {}", Join(versions, 3), target);
                for (const UfScript* s : other)
                    p.files.push_back({OpKind::MoveToBackup, it.id, {}, s->file.lexically_relative(r.dir), CopyMode::Replace, 0, 0});
                installScript();
            } else {
                it.status = F("оставить {}", "keep {}", Join(versions, 3));
            }
        } else if (!same.empty() || r.ufExactInstalled) {
            it.detail = T("Скрипт уже установлен. Отметьте, чтобы записать его заново.", "The script is already installed. Check to write it again.");
            if (b.Optional(it, false, ItemState::Update)) {
                it.status = F("переустановить {}", "reinstall {}", target);
                installScript();
            } else if (b.AnyMissing(Comp::Config)) {
                it.state = ItemState::Repair;
                it.status = F("{}, не хватает файлов настроек", "{}, settings files missing", target);
                b.Copy(it.id, Comp::Config, CopyMode::AddIfMissing);
            } else {
                it.state = ItemState::Ok;
                it.status = F("установлен {}", "{} installed", target);
            }
        } else {
            it.status = T("не установлен", "not installed");
            it.detail = F("UltraFuck {} и его настройки.", "UltraFuck {} and its settings.", target);
            if (b.Optional(it, true, ItemState::Install)) {
                it.status = F("будет установлен {}", "{} will be installed", target);
                installScript();
            }
        }
        for (const UfScript* s : inert)
            b.Note(Severity::Info, F("Найден «{}» — формат .aluac MoonLoader не загружает, файл не трогаем.",
                                     "Found \"{}\": MoonLoader does not load the .aluac format, the file is left as is.",
                                     PathUtf8(s->file.filename())));
    }

    // ---- Fonts
    {
        PlanItem& it = b.Add(ItemId::Fonts, T("Шрифты Trebuchet MS", "Trebuchet MS fonts"));
        std::vector<std::string> missing;
        bool requiredMissing = false;
        for (const FontState& f : r.fonts) {
            if (f.bundled && !f.present) {
                missing.push_back(f.file);
                requiredMissing |= f.required;
            }
        }
        if (missing.empty()) {
            it.status = T("установлены", "installed");
        } else {
            it.status = T("нет: ", "missing: ") + Join(missing);
            it.detail = T("Шрифты будут установлены в Windows. trebucbd.ttf обязателен — без него меню скрипта (imgui) не откроется.",
                          "The fonts will be installed into Windows. trebucbd.ttf is required: without it the script menu (imgui) "
                          "does not open.");
            it.admin = !o.customFontsDir;
            if (b.Optional(it, true, ItemState::Install)) {
                for (const PayloadFont& f : gen::kFonts)
                    if (std::find(missing.begin(), missing.end(), f.file) != missing.end())
                        p.fonts.push_back({f.entry, f.file, f.regName, f.size, f.crc});
            } else if (requiredMissing) {
                b.Note(Severity::Error, T("Без шрифта trebucbd.ttf (Trebuchet MS Bold) меню скрипта не откроется.",
                                          "Without the trebucbd.ttf font (Trebuchet MS Bold) the script menu does not open."));
            }
        }
        for (const FontState& f : r.fonts)
            if (!f.bundled && !f.present)
                b.Note(Severity::Warning, F("В Windows нет шрифта {} ({}) — часть надписей в игре будет другим шрифтом.",
                                            "Windows has no {} font ({}): some text in the game will use another font.", f.name, f.file));
    }

    // ---- DirectX 9 (d3dx9_43.dll)
    {
        PlanItem& it = b.Add(ItemId::DirectX, "DirectX 9");
        if (r.d3dx9) {
            it.status = T("d3dx9_43.dll найден", "d3dx9_43.dll found");
        } else {
            it.status = T("нет d3dx9_43.dll", "no d3dx9_43.dll");
            it.detail = T("Будет скачан и запущен официальный веб-установщик DirectX от Microsoft (нужен интернет).",
                          "The official DirectX web installer from Microsoft will be downloaded and run (needs internet).");
            it.admin = true;
            if (b.Optional(it, true, ItemState::Install))
                p.directx = true;
            else
                b.Note(Severity::Error, T("Без d3dx9_43.dll MoonLoader и SAMPFUNCS не загрузятся.",
                                          "Without d3dx9_43.dll MoonLoader and SAMPFUNCS will not load."));
        }
    }

    // ---- Environment
    if (r.gameRunning) {
        p.blocked = true;
        p.blockReason = T("Игра запущена из этой папки — закройте её перед установкой.",
                          "The game is running from this folder: close it before installing.");
    } else if (r.gameMaybeRunning) {
        b.Note(Severity::Warning, T("Запущен gta_sa.exe — если это игра из этой папки, закройте её перед установкой.",
                                    "gta_sa.exe is running: if it is the game from this folder, close it before installing."));
    }
    if (r.nonAsciiPath)
        b.Note(Severity::Warning, T("В пути к игре есть русские буквы или спецсимволы. MoonLoader и SAMPFUNCS с такими путями "
                                    "часто работают с ошибками — лучше перенести игру в папку вида C:\\Games\\GTA.",
                                    "The game path contains non-Latin letters or special characters. MoonLoader and SAMPFUNCS often "
                                    "misbehave with such paths: better move the game to a folder like C:\\Games\\GTA."));
    if (az)
        b.Note(Severity::Info, F("Это папка {} из Arizona Launcher. Загрузчик, CLEO, SAMPFUNCS и bass.dll обновляет сам "
                                 "лаунчер — установщик их не трогает.",
                                 "This is the {} folder of Arizona Launcher. The loader, CLEO, SAMPFUNCS and bass.dll are updated by "
                                 "the launcher itself, so the installer leaves them alone.",
                                 ArizonaTitle(r.arizonaId)));
    if (r.arizonaAutoClean)
        b.Note(Severity::Warning, T("В Arizona Launcher включена «Автоочистка» — она может удалить установленные файлы. "
                                    "Отключите её в настройках игры в лаунчере.",
                                    "\"Auto clean\" is on in Arizona Launcher: it may delete the installed files. "
                                    "Turn it off in the game settings of the launcher."));
    if (!r.virtualStoreDir.empty())
        b.Note(Severity::Warning, F("Часть файлов игры может лежать в VirtualStore: {}", "Some game files may be in VirtualStore: {}",
                                    PathUtf8(r.virtualStoreDir)));

    // ---- Administrator rights
    if (!p.fonts.empty() && !o.customFontsDir) p.adminReasons.push_back(T("установка шрифтов в Windows", "installing fonts into Windows"));
    if (p.directx) p.adminReasons.push_back(T("установка DirectX", "installing DirectX"));
    if ((!p.files.empty() || !p.dirs.empty()) && !r.writable) {
        p.adminReasons.push_back(T("запись в папку игры", "writing to the game folder"));
        for (PlanItem& it : p.items)
            if (std::any_of(p.files.begin(), p.files.end(), [&](const FileOp& f) { return f.item == it.id; })) it.admin = true;
    }
    p.needsAdmin = !p.adminReasons.empty();
    return p;
}

std::uint64_t InstallPlan::Hash() const {
    std::uint64_t h = 1469598103934665603ull;
    auto mix = [&](const void* data, std::size_t n) {
        const auto* bytes = static_cast<const unsigned char*>(data);
        for (std::size_t i = 0; i < n; ++i) h = (h ^ bytes[i]) * 1099511628211ull;
    };
    auto mixStr = [&](const std::string& s) { mix(s.data(), s.size() + 1); };
    for (const FileOp& f : files) {
        int k = static_cast<int>(f.kind) * 2 + static_cast<int>(f.mode);
        mix(&k, sizeof(k));
        mixStr(f.entry);
        mixStr(ToUtf8(ToLower(f.rel.native())));
    }
    for (const fs::path& d : dirs) mixStr(ToUtf8(ToLower(d.native())));
    for (const FontOp& f : fonts) mixStr(f.file);
    mix(&directx, sizeof(directx));
    return h;
}

std::uint64_t InstallPlan::BytesToWrite() const {
    std::uint64_t n = 0;
    for (const FileOp& f : files) n += f.size;
    for (const FontOp& f : fonts) n += f.size;
    return n;
}

const PlanItem* InstallPlan::Find(ItemId id) const {
    for (const PlanItem& it : items)
        if (it.id == id) return &it;
    return nullptr;
}

std::string EncodeOptions(const Options& o) {
    std::string s;
    for (std::size_t i = 0; i < kItemCount; ++i)
        if (o.toggles[i]) s += std::format("t{}={};", i, *o.toggles[i] ? 1 : 0);
    // Always written: the GUI turns it on by default, so "off" has to survive the elevated relaunch.
    s += o.overwriteLibs ? "ol=1;" : "ol=0;";
    if (o.customFontsDir) s += "cf=1;";
    return s;
}

Options DecodeOptions(std::string_view s) {
    Options o;
    while (!s.empty()) {
        auto semi = s.find(';');
        std::string_view kv = s.substr(0, semi);
        s = semi == std::string_view::npos ? std::string_view{} : s.substr(semi + 1);
        auto eq = kv.find('=');
        if (eq == std::string_view::npos) continue;
        std::string_view key = kv.substr(0, eq), val = kv.substr(eq + 1);
        bool on = val == "1";
        if (key == "ol") {
            o.overwriteLibs = on;
        } else if (key == "cf") {
            o.customFontsDir = on;
        } else if (key.size() > 1 && key[0] == 't') {
            std::size_t idx = 0;
            auto res = std::from_chars(key.data() + 1, key.data() + key.size(), idx);
            if (res.ec == std::errc() && idx < kItemCount) o.toggles[idx] = on;
        }
    }
    return o;
}

}  // namespace uf
