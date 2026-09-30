#include "core/inspect.h"

#include <windows.h>
#include <tlhelp32.h>

#include <algorithm>
#include <map>

#include "core/directx.h"
#include "core/fonts.h"
#include "core/fsutil.h"
#include "core/i18n.h"
#include "core/pe.h"
#include "core/versions.h"
#include "payload_manifest.gen.h"

namespace uf {

namespace {

// Real on-disk name (for display), e.g. "CLEO.asi" vs "cleo.asi".
fs::path ActualName(const fs::path& p) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(p.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return p;
    FindClose(h);
    return p.parent_path() / fd.cFileName;
}

std::optional<fs::path> FindAsi(const fs::path& dir, const wchar_t* name) {
    for (const fs::path& p : {dir / name, dir / L"scripts" / name})
        if (FileExists(p)) return ActualName(p);
    return std::nullopt;
}

std::string TrimVersion(std::string v) {
    // "4.3.22.0" -> "4.3.22"
    while (v.size() > 2 && v.ends_with(".0") && std::count(v.begin(), v.end(), '.') > 2) v.resize(v.size() - 2);
    return v;
}

void DetectLoader(const fs::path& d, FolderReport& r) {
    r.hasHookedVorbis = FileExists(d / L"vorbisHooked.dll");

    std::string ual;
    static const wchar_t* kProxies[] = {L"dinput8.dll", L"d3d8.dll",   L"d3d9.dll",    L"d3d10.dll",   L"d3d11.dll",
                                        L"dxgi.dll",    L"dsound.dll", L"winmm.dll",   L"winhttp.dll", L"wininet.dll",
                                        L"version.dll", L"xlive.dll",  L"msacm32.dll", L"msvfw32.dll"};
    for (const wchar_t* proxy : kProxies) {
        fs::path p = d / proxy;
        auto size = FileSize(p);
        if (!size || *size > (32u << 20)) continue;
        if (auto bytes = ReadFileBytes(p); bytes && (pe::ContainsText(*bytes, "Ultimate ASI Loader") ||
                                                     pe::ContainsText(*bytes, "Ultimate-ASI-Loader"))) {
            ual = "Ultimate ASI Loader (" + ToUtf8(proxy) + ")";
            break;
        }
    }

    fs::path vf = d / L"vorbisFile.dll";
    auto size = FileSize(vf);
    if (!size) {
        r.loader = ual.empty() ? LoaderKind::Missing : LoaderKind::Ual;
        r.ualName = ual;
        return;
    }
    auto crc = FileCrc32(vf).value_or(0);
    bool hookedName = false;
    if (*size < (8u << 20))
        if (auto bytes = ReadFileBytes(vf)) hookedName = pe::ContainsText(*bytes, "vorbisHooked");
    LoaderKind kind = ClassifyVorbisFile(*size, crc, hookedName);
    if (kind == LoaderKind::Silent && !r.hasHookedVorbis) kind = LoaderKind::SilentNoHooked;
    if (kind == LoaderKind::None && !ual.empty()) kind = LoaderKind::Ual;
    r.loader = kind;
    if (kind == LoaderKind::Ual) r.ualName = ual;
}

void DetectUfScripts(const fs::path& d, FolderReport& r) {
    fs::path ml = d / L"moonloader";
    if (!DirExists(ml)) return;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileExW((ml / L"*").c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, 0);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        std::wstring name = fd.cFileName;
        fs::path file = ml / name;
        if (auto parsed = ParseUfScriptName(name)) {
            r.ufScripts.push_back({file, parsed->version, parsed->ext != L"aluac"});
        } else if (IEndsWith(name, L".lua")) {
            if (auto head = ReadFileBytes(file, 4096)) {
                std::string text(head->begin(), head->end());
                if (LuaDeclaresUltraFuck(text)) r.ufScripts.push_back({file, "", true});
            }
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(r.ufScripts.begin(), r.ufScripts.end(), [](const UfScript& a, const UfScript& b) { return a.file < b.file; });
}

}  // namespace

std::size_t FolderReport::MissingUnitCount() const {
    return static_cast<std::size_t>(std::count_if(libUnits.begin(), libUnits.end(), [](const LibUnitState& u) { return !u.anyPresent; }));
}

bool HasGtaExe(const fs::path& dir) { return FileExists(dir / L"gta_sa.exe"); }

GtaVersion DetectGtaVersion(const fs::path& exe) {
    if (!FileExists(exe)) return GtaVersion::Missing;
    auto info = pe::ParseFile(exe);
    if (!info) return GtaVersion::Unknown;
    for (const GtaSignature& sig : kGtaSignatures) {
        auto off = pe::VaToFileOffset(*info, sig.va);
        if (!off) continue;
        auto bytes = ReadFileRange(exe, *off, 4);
        if (!bytes || bytes->size() != 4) continue;
        std::uint32_t v = (*bytes)[0] | ((*bytes)[1] << 8) | ((*bytes)[2] << 16) | (static_cast<std::uint32_t>((*bytes)[3]) << 24);
        if (v == kGtaSignatureValue) return sig.version;
    }
    return GtaVersion::Unknown;
}

LoaderKind ClassifyVorbisFile(std::uint64_t size, std::uint32_t crc, bool containsHookedName) {
    for (const KnownBinary& k : gen::kSilentLoaders)
        if (k.size == size && k.crc == crc) return LoaderKind::Silent;
    if (gen::kOriginalVorbisFile.size == size && gen::kOriginalVorbisFile.crc == crc) return LoaderKind::None;
    return containsHookedName ? LoaderKind::Silent : LoaderKind::Other;
}

void DetectRunningGame(const fs::path& dir, bool* running, bool* maybe) {
    *running = *maybe = false;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    std::wstring key = PathKey(dir);
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe)) {
        if (!IEquals(pe.szExeFile, L"gta_sa.exe")) continue;
        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
        if (!h) {
            *maybe = true;
            continue;
        }
        std::wstring buf(32768, L'\0');
        DWORD n = static_cast<DWORD>(buf.size());
        if (QueryFullProcessImageNameW(h, 0, buf.data(), &n)) {
            buf.resize(n);
            if (PathKey(fs::path(buf).parent_path()) == key) *running = true;
        } else {
            *maybe = true;
        }
        CloseHandle(h);
    }
    CloseHandle(snap);
}

std::string LoaderName(const FolderReport& r) {
    switch (r.loader) {
        case LoaderKind::None: return T("не установлен", "not installed");
        case LoaderKind::Silent: return "Silent's ASI Loader";
        case LoaderKind::SilentNoHooked: return T("Silent's ASI Loader без vorbisHooked.dll", "Silent's ASI Loader without vorbisHooked.dll");
        case LoaderKind::Ual: return r.ualName;
        case LoaderKind::Other: return T("сторонний загрузчик (vorbisFile.dll)", "third-party loader (vorbisFile.dll)");
        case LoaderKind::Missing: return T("vorbisFile.dll отсутствует", "vorbisFile.dll is missing");
    }
    return {};
}

FolderReport Inspect(const fs::path& dirIn, const InspectOptions& opt) {
    FolderReport r;
    r.dir = CanonicalPath(dirIn);
    const fs::path& d = r.dir;

    r.hasGtaExe = HasGtaExe(d);
    r.gta = DetectGtaVersion(d / L"gta_sa.exe");

    if (fs::path samp = d / L"samp.dll"; FileExists(samp)) {
        r.hasSamp = true;
        auto info = pe::ParseFile(samp);
        r.samp = info ? SampVersionFromEntryPoint(info->entryRva) : SampVersion::Unknown;
        r.sampFileVersion = TrimVersion(pe::FileVersion(samp));
    }

    DetectLoader(d, r);

    if (auto p = FindAsi(d, L"cleo.asi")) r.cleo = AsiInfo{*p, TrimVersion(pe::FileVersion(*p)), {}};
    if (auto p = FindAsi(d, L"SAMPFUNCS.asi")) {
        AsiInfo info{*p, {}, {}};
        if (auto bytes = ReadFileBytes(*p)) {
            std::string banner = pe::FindAsciiString(*bytes, "SAMPFUNCS v");
            if (banner.size() > 11) {
                std::string rest = banner.substr(11);
                auto paren = rest.find(" (");
                info.version = rest.substr(0, paren);
                if (paren != std::string::npos) {
                    auto end = rest.find(')', paren);
                    info.target = rest.substr(paren + 2, end == std::string::npos ? std::string::npos : end - paren - 2);
                    if (info.target.starts_with("SA-MP ")) info.target = info.target.substr(6);
                }
            }
        }
        r.sampfuncs = info;
    }
    if (auto p = FindAsi(d, L"MoonLoader.asi")) {
        AsiInfo info{*p, {}, {}};
        if (auto bytes = ReadFileBytes(*p)) {
            // "MoonLoader v.026.5-beta loaded." -> "026.5-beta"
            std::string banner = pe::FindAsciiString(*bytes, "MoonLoader v.");
            if (banner.size() > 13) info.version = banner.substr(13, banner.find(' ', 13) - 13);
        }
        r.moonloader = info;
    }
    if (fs::path bass = d / L"bass.dll"; FileExists(bass)) {
        r.hasBass = true;
        r.bassVersion = pe::FileVersion(bass);
    }
    r.hasLua51 = FileExists(d / L"lua51.dll");

    // Payload files / dirs / library units.
    r.payloadFileExists.resize(std::size(gen::kFiles));
    r.payloadFileSame.resize(std::size(gen::kFiles));
    std::map<std::string, LibUnitState> units;
    for (std::size_t i = 0; i < std::size(gen::kFiles); ++i) {
        const PayloadFile& f = gen::kFiles[i];
        bool exists = FileExists(d / ToWide(f.dest));
        r.payloadFileExists[i] = exists;
        if (f.comp == Comp::Lib) {
            // Small Lua files: comparing them all takes a few milliseconds.
            r.payloadFileSame[i] = exists && FileMatches(d / ToWide(f.dest), f.size, f.crc);
            auto [it, inserted] = units.try_emplace(f.unit, LibUnitState{f.unit, false, true});
            it->second.anyPresent |= exists;
            it->second.complete &= exists;
        }
        if (f.comp == Comp::Script) r.ufExactInstalled = exists && FileMatches(d / ToWide(f.dest), f.size, f.crc);
    }
    for (auto& [name, state] : units) r.libUnits.push_back(state);
    r.payloadDirExists.resize(std::size(gen::kDirs));
    for (std::size_t i = 0; i < std::size(gen::kDirs); ++i) r.payloadDirExists[i] = DirExists(d / ToWide(gen::kDirs[i].dest));

    DetectUfScripts(d, r);

    r.d3dx9 = !opt.simulateMissingD3dx9 && HasD3DX9_43(d);

    fs::path fontsDir = opt.fontsDir ? *opt.fontsDir : SystemFontsDir();
    for (const PayloadFont& f : gen::kFonts) {
        std::string name = f.regName;
        if (auto p = name.rfind(" ("); p != std::string::npos) name.resize(p);
        r.fonts.push_back({f.file, name, !opt.simulateMissingFonts && FileExists(fontsDir / ToWide(f.file)), f.required, true});
    }
    for (const WarnFont& f : kWarnFonts)
        r.fonts.push_back({f.file, f.name, FileExists(SystemFontsDir() / ToWide(f.file)), false, false});

    DetectRunningGame(d, &r.gameRunning, &r.gameMaybeRunning);

    if (opt.arizona) {
        std::wstring key = PathKey(d);
        for (const ArizonaGame& g : opt.arizona->games) {
            if (PathKey(g.dir) == key) {
                r.arizona = true;
                r.arizonaId = g.id;
                r.arizonaAutoClean = g.autoClean;
            }
        }
    }
    if (!r.arizona) {
        if (auto id = ArizonaIdFromLayout(d)) {
            r.arizona = true;
            r.arizonaId = *id;
        }
    }

    r.nonAsciiPath = !IsAscii(d.native());
    r.underProgramFiles = IsUnderProgramFiles(d);
    if (r.underProgramFiles && d.has_root_name()) {
        fs::path vs = GetFolder(Folder::LocalAppData) / L"VirtualStore" / d.relative_path();
        if (DirExists(vs)) r.virtualStoreDir = vs;
    }
    r.writable = IsDirWritable(d);
    for (const wchar_t* sub : {L"moonloader", L"moonloader\\lib", L"moonloader\\config"})
        if (r.writable && DirExists(d / sub)) r.writable = IsDirWritable(d / sub);
    return r;
}

}  // namespace uf
