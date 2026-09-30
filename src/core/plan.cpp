#include "core/plan.h"

#include <algorithm>
#include <charconv>
#include <format>
#include <functional>
#include <set>

#include "core/arizona.h"
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
    if (parts.size() > limit) out += std::format(" и ещё {}", parts.size() - limit);
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

    void Copy(ItemId item, Comp comp, CopyMode mode, const std::function<bool(const PayloadFile&)>& filter = {}) {
        for (std::size_t i = 0; i < std::size(gen::kFiles); ++i) {
            const PayloadFile& f = gen::kFiles[i];
            if (f.comp != comp || (filter && !filter(f))) continue;
            fs::path rel = ToWide(f.dest);
            if (r_.arizona && IsArizonaManagedFile(rel)) continue;
            if (mode == CopyMode::AddIfMissing && Exists(i)) continue;
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

    // ---- GTA SA
    {
        PlanItem& it = b.Add(ItemId::Gta, "GTA San Andreas");
        if (!r.hasGtaExe) {
            it.state = ItemState::Error;
            it.status = "gta_sa.exe не найден";
            p.blocked = true;
            p.blockReason = "В выбранной папке нет gta_sa.exe.";
        } else if (r.gta == GtaVersion::US10) {
            it.status = "версия 1.0 US";
        } else {
            it.state = ItemState::Warning;
            it.status = std::format("версия {}", GtaVersionName(r.gta));
            it.detail = "MoonLoader, CLEO и SAMPFUNCS работают только с gta_sa.exe версии 1.0 US.";
            b.Note(Severity::Warning, "gta_sa.exe не версии 1.0 US — MoonLoader и SAMPFUNCS могут не запуститься. "
                                      "Нужен gta_sa.exe 1.0 US (даунгрейд).");
        }
    }

    // ---- SA-MP (info only)
    {
        PlanItem& it = b.Add(ItemId::Samp, "SA-MP");
        if (!r.hasSamp) {
            it.state = ItemState::Error;
            it.status = "не установлен";
            it.detail = "samp.dll не найден.";
            b.Note(Severity::Error, "SA-MP не найден. Установите клиент SA-MP 0.3.7 — без него скрипт работать не будет.");
        } else if (r.samp == SampVersion::Unknown) {
            it.state = ItemState::Warning;
            it.status = std::format("неизвестная сборка {}", r.sampFileVersion);
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
                it.status = r.loaderName;
                break;
            case LoaderKind::Other:
                it.status = az ? "загрузчик Arizona" : r.loaderName;
                if (az) it.state = ItemState::Managed;
                break;
            case LoaderKind::SilentNoHooked:
                it.status = "нет vorbisHooked.dll";
                if (az) {
                    it.state = ItemState::Managed;
                    break;
                }
                it.detail = "Будет добавлен оригинальный vorbisHooked.dll — без него в игре нет звука.";
                if (b.Optional(it, true, ItemState::Repair))
                    b.Copy(it.id, Comp::AsiLoader, CopyMode::AddIfMissing, [](const PayloadFile& f) { return DestIs(f, L"vorbisHooked.dll"); });
                break;
            case LoaderKind::None:
            case LoaderKind::Missing:
                it.status = r.loader == LoaderKind::None ? "не установлен" : "vorbisFile.dll отсутствует";
                if (az) {
                    it.state = ItemState::Error;
                    it.detail = "Загрузчиком управляет Arizona Launcher — запустите в нём проверку файлов игры.";
                    b.Note(Severity::Error, "Нет загрузчика ASI. Запустите проверку файлов игры в Arizona Launcher.");
                    break;
                }
                it.detail = "Silent's ASI Loader 1.3 — без него игра не загружает CLEO, SAMPFUNCS и MoonLoader. "
                            "Оригинальный vorbisFile.dll уйдёт в резервную копию.";
                if (b.Optional(it, true, ItemState::Install)) {
                    b.Copy(it.id, Comp::AsiLoader, CopyMode::Replace, [](const PayloadFile& f) { return DestIs(f, L"vorbisFile.dll"); });
                    b.Copy(it.id, Comp::AsiLoader, CopyMode::AddIfMissing, [](const PayloadFile& f) { return !DestIs(f, L"vorbisFile.dll"); });
                } else {
                    b.Note(Severity::Warning, "Без ASI Loader игра не загрузит CLEO, SAMPFUNCS и MoonLoader.");
                }
                break;
        }
    }

    // ---- CLEO
    bool cleoAfter = r.cleo.has_value();
    {
        PlanItem& it = b.Add(ItemId::Cleo, "CLEO");
        if (r.cleo) {
            it.status = r.cleo->version.empty() ? "установлен" : r.cleo->version;
        } else if (az) {
            it.state = ItemState::Error;
            it.status = "не найден";
            it.detail = "CLEO ставит Arizona Launcher — запустите в нём проверку файлов игры.";
            b.Note(Severity::Error, "CLEO не найден. Запустите проверку файлов игры в Arizona Launcher.");
        } else {
            it.status = "не установлен";
            it.detail = std::format("CLEO {} — нужен для SAMPFUNCS.", ShortVersion(gen::kCleoVersion));
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
            it.status = r.sampfuncs->version.empty() ? "установлен" : r.sampfuncs->version;
            auto targets = SampVersionsFromText(r.sampfuncs->target);
            if (r.hasSamp && r.samp != SampVersion::Unknown && !targets.empty() &&
                std::find(targets.begin(), targets.end(), r.samp) == targets.end()) {
                it.state = ItemState::Warning;
                it.detail = std::format("Эта сборка SAMPFUNCS для SA-MP {}, а у вас {}.", r.sampfuncs->target, SampVersionName(r.samp));
                b.Note(Severity::Warning, std::format("SAMPFUNCS рассчитан на SA-MP {}, а установлен {} — скрипт может не работать.",
                                                      r.sampfuncs->target, SampVersionName(r.samp)));
            }
        } else if (az) {
            it.state = ItemState::Error;
            it.status = "не найден";
            it.detail = "SAMPFUNCS ставит Arizona Launcher — запустите в нём проверку файлов игры.";
            b.Note(Severity::Error, "SAMPFUNCS не найден. Запустите проверку файлов игры в Arizona Launcher.");
        } else if (!r.hasSamp) {
            it.state = ItemState::Error;
            it.status = "не установлен";
            it.detail = std::format("Сначала установите SA-MP {} — SAMPFUNCS {} работает только с ним.", gen::kSampfuncsTarget,
                                    ShortVersion(gen::kSampfuncsVersion));
        } else if (r.samp == SampVersion::R1) {
            it.status = "не установлен";
            it.detail = std::format("SAMPFUNCS {} для SA-MP {}.", gen::kSampfuncsVersion, gen::kSampfuncsTarget);
            if (b.Optional(it, true, ItemState::Install)) {
                b.Copy(it.id, Comp::Sampfuncs, CopyMode::AddIfMissing);
                b.Dirs(Comp::Sampfuncs);
                if (!cleoAfter) b.Note(Severity::Warning, "SAMPFUNCS не загрузится без CLEO.");
            } else {
                b.Note(Severity::Error, "Без SAMPFUNCS скрипт работать не будет.");
            }
        } else {
            it.state = ItemState::Error;
            it.status = "не установлен";
            it.detail = std::format("В комплекте SAMPFUNCS {} — он работает только с SA-MP {}, а у вас {}.",
                                    gen::kSampfuncsVersion, gen::kSampfuncsTarget, SampVersionName(r.samp));
            b.Note(Severity::Error, std::format("SAMPFUNCS не установлен, а версия из комплекта подходит только к SA-MP {} (у вас {}). "
                                                "Без SAMPFUNCS для вашей версии SA-MP скрипт работать не будет.",
                                                gen::kSampfuncsTarget, SampVersionName(r.samp)));
        }
    }

    // ---- MoonLoader
    bool mlAfter = r.moonloader.has_value();
    bool mlFresh = false;
    {
        PlanItem& it = b.Add(ItemId::MoonLoader, "MoonLoader");
        if (r.moonloader) {
            it.status = r.moonloader->version.empty() ? "установлен" : r.moonloader->version;
            int major = MoonLoaderMajor(r.moonloader->version);
            if (major >= 0 && major < 26) {
                it.detail = std::format("Устаревшая версия — скрипту нужен MoonLoader 026+. Будет установлен {}.", gen::kMoonLoaderVersion);
                if (b.Optional(it, true, ItemState::Update))
                    b.Copy(it.id, Comp::MoonLoader, CopyMode::Replace);
                else
                    it.state = ItemState::Warning;
            } else if (!r.hasLua51) {
                it.status = "нет lua51.dll";
                it.detail = "Будет добавлен lua51.dll — без него MoonLoader не запустится.";
                if (b.Optional(it, true, ItemState::Repair))
                    b.Copy(it.id, Comp::MoonLoader, CopyMode::AddIfMissing, [](const PayloadFile& f) { return DestIs(f, L"lua51.dll"); });
            }
        } else {
            it.status = "не установлен";
            it.detail = std::format("MoonLoader {} с lua51.dll и стандартными скриптами.", gen::kMoonLoaderVersion);
            if (b.Optional(it, true, ItemState::Install)) {
                b.Copy(it.id, Comp::MoonLoader, CopyMode::AddIfMissing);
                b.Copy(it.id, Comp::MoonLoaderScripts, CopyMode::AddIfMissing);
                mlAfter = mlFresh = true;
            } else {
                b.Note(Severity::Error, "Без MoonLoader скрипт работать не будет.");
            }
        }
    }

    // ---- bass.dll: imported by CLEO.asi and MoonLoader.asi
    if (!r.hasBass && !az && (cleoAfter || mlAfter)) {
        ItemId owner = mlFresh ? ItemId::MoonLoader : ItemId::Cleo;
        std::size_t before = p.files.size();
        b.Copy(owner, Comp::Bass, CopyMode::AddIfMissing);
        if (p.files.size() > before) b.Note(Severity::Info, "Будет добавлен bass.dll — он нужен CLEO и MoonLoader.");
    }

    // ---- Libraries
    {
        PlanItem& it = b.Add(ItemId::Libs, "Библиотеки MoonLoader");
        std::set<std::string> missing;
        std::vector<std::string> names;
        for (const LibUnitState& u : r.libUnits)
            if (!u.anyPresent) {
                missing.insert(u.unit);
                names.push_back(u.unit);
            }
        if (o.overwriteLibs) {
            it.status = "будут перезаписаны";
            it.detail = "Все библиотеки из комплекта будут записаны заново, изменённые — сохранены в резервную копию.";
            if (b.Optional(it, true, ItemState::Update)) b.Copy(it.id, Comp::Lib, CopyMode::Replace);
        } else if (!missing.empty()) {
            it.status = std::format("не хватает {} из {}", missing.size(), r.libUnits.size());
            it.detail = "Будут добавлены: " + Join(names);
            if (b.Optional(it, true, ItemState::Install))
                b.Copy(it.id, Comp::Lib, CopyMode::AddIfMissing, [&](const PayloadFile& f) { return missing.contains(f.unit); });
            else
                b.Note(Severity::Warning, "Без недостающих библиотек скрипт может не запуститься.");
        } else {
            it.status = "все на месте";
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
            it.detail = "Старые файлы будут перенесены в резервную копию: " + Join(files) + ". Настройки (Settings.ini) сохранятся.";
            if (b.Optional(it, true, ItemState::Update)) {
                it.status = std::format("{} → {}", Join(versions, 3), target);
                for (const UfScript* s : other)
                    p.files.push_back({OpKind::MoveToBackup, it.id, {}, s->file.lexically_relative(r.dir), CopyMode::Replace, 0, 0});
                installScript();
            } else {
                it.status = std::format("оставить {}", Join(versions, 3));
            }
        } else if (!same.empty() || r.ufExactInstalled) {
            it.detail = "Скрипт уже установлен. Отметьте, чтобы записать его заново.";
            if (b.Optional(it, false, ItemState::Update)) {
                it.status = std::format("переустановить {}", target);
                installScript();
            } else if (b.AnyMissing(Comp::Config)) {
                it.state = ItemState::Repair;
                it.status = std::format("{}, не хватает файлов настроек", target);
                b.Copy(it.id, Comp::Config, CopyMode::AddIfMissing);
            } else {
                it.state = ItemState::Ok;
                it.status = std::format("установлен {}", target);
            }
        } else {
            it.status = "не установлен";
            it.detail = std::format("UltraFuck {} и его настройки.", target);
            if (b.Optional(it, true, ItemState::Install)) {
                it.status = std::format("будет установлен {}", target);
                installScript();
            }
        }
        for (const UfScript* s : inert)
            b.Note(Severity::Info, std::format("Найден «{}» — формат .aluac MoonLoader не загружает, файл не трогаем.", PathUtf8(s->file.filename())));
    }

    // ---- Fonts
    {
        PlanItem& it = b.Add(ItemId::Fonts, "Шрифты Trebuchet MS");
        std::vector<std::string> missing;
        bool requiredMissing = false;
        for (const FontState& f : r.fonts) {
            if (f.bundled && !f.present) {
                missing.push_back(f.file);
                requiredMissing |= f.required;
            }
        }
        if (missing.empty()) {
            it.status = "установлены";
        } else {
            it.status = "нет: " + Join(missing);
            it.detail = "Шрифты будут установлены в Windows. trebucbd.ttf обязателен — без него меню скрипта (imgui) не откроется.";
            it.admin = !o.customFontsDir;
            if (b.Optional(it, true, ItemState::Install)) {
                for (const PayloadFont& f : gen::kFonts)
                    if (std::find(missing.begin(), missing.end(), f.file) != missing.end())
                        p.fonts.push_back({f.entry, f.file, f.regName, f.size, f.crc});
            } else if (requiredMissing) {
                b.Note(Severity::Error, "Без шрифта trebucbd.ttf (Trebuchet MS Bold) меню скрипта не откроется.");
            }
        }
        for (const FontState& f : r.fonts)
            if (!f.bundled && !f.present)
                b.Note(Severity::Warning, std::format("В Windows нет шрифта {} ({}) — часть надписей в игре будет другим шрифтом.", f.name, f.file));
    }

    // ---- DirectX 9 (d3dx9_43.dll)
    {
        PlanItem& it = b.Add(ItemId::DirectX, "DirectX 9");
        if (r.d3dx9) {
            it.status = "d3dx9_43.dll найден";
        } else {
            it.status = "нет d3dx9_43.dll";
            it.detail = "Будет скачан и запущен официальный веб-установщик DirectX от Microsoft (нужен интернет).";
            it.admin = true;
            if (b.Optional(it, true, ItemState::Install))
                p.directx = true;
            else
                b.Note(Severity::Error, "Без d3dx9_43.dll MoonLoader и SAMPFUNCS не загрузятся.");
        }
    }

    // ---- Environment
    if (r.gameRunning) {
        p.blocked = true;
        p.blockReason = "Игра запущена из этой папки — закройте её перед установкой.";
    } else if (r.gameMaybeRunning) {
        b.Note(Severity::Warning, "Запущен gta_sa.exe — если это игра из этой папки, закройте её перед установкой.");
    }
    if (r.nonAsciiPath)
        b.Note(Severity::Warning, "В пути к игре есть русские буквы или спецсимволы. MoonLoader и SAMPFUNCS с такими путями "
                                  "часто работают с ошибками — лучше перенести игру в папку вида C:\\Games\\GTA.");
    if (az)
        b.Note(Severity::Info, std::format("Это папка {} из Arizona Launcher. Загрузчик, CLEO, SAMPFUNCS и bass.dll обновляет сам "
                                           "лаунчер — установщик их не трогает.", r.arizonaTitle));
    if (r.arizonaAutoClean)
        b.Note(Severity::Warning, "В Arizona Launcher включена «Автоочистка» — она может удалить установленные файлы. "
                                  "Отключите её в настройках игры в лаунчере.");
    if (!r.virtualStoreDir.empty())
        b.Note(Severity::Warning, std::format("Часть файлов игры может лежать в VirtualStore: {}", PathUtf8(r.virtualStoreDir)));

    // ---- Administrator rights
    if (!p.fonts.empty() && !o.customFontsDir) p.adminReasons.push_back("установка шрифтов в Windows");
    if (p.directx) p.adminReasons.push_back("установка DirectX");
    if ((!p.files.empty() || !p.dirs.empty()) && !r.writable) {
        p.adminReasons.push_back("запись в папку игры");
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
    if (o.overwriteLibs) s += "ol=1;";
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
