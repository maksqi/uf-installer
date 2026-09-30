#pragma once
// Read-only analysis of a GTA SA folder: what is installed and in which versions.
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/arizona.h"
#include "core/known.h"
#include "core/paths.h"

namespace uf {

enum class LoaderKind {
    None,            // original vorbisFile.dll, no ASI loader at all
    Missing,         // vorbisFile.dll is missing (game is broken)
    Silent,          // Silent's ASI Loader
    SilentNoHooked,  // Silent's loader without vorbisHooked.dll (no sound / crash)
    Ual,             // Ultimate ASI Loader proxy dll
    Other,           // non-original vorbisFile.dll (Arizona's loader, other builds)
};

struct AsiInfo {
    fs::path file;
    std::string version;  // "4.3.22", "026.5-beta", "5.4.1-final rel.21"
    std::string target;   // SAMPFUNCS only: "0.3.7-R1"
};

struct UfScript {
    fs::path file;
    std::string version;  // "" if unknown
    bool loadable = true; // .lua/.luac are loaded by MoonLoader, .aluac is not
};

struct FontState {
    std::string file;
    std::string name;
    bool present = false;
    bool required = false;
    bool bundled = false;  // can be installed from the payload
};

struct LibUnitState {
    std::string unit;
    bool anyPresent = false;
    bool complete = false;
};

struct FolderReport {
    fs::path dir;
    bool hasGtaExe = false;
    GtaVersion gta = GtaVersion::Missing;

    bool hasSamp = false;
    SampVersion samp = SampVersion::None;
    std::string sampFileVersion;

    LoaderKind loader = LoaderKind::None;
    std::string ualName;  // "Ultimate ASI Loader (dinput8.dll)" when loader == Ual
    bool hasHookedVorbis = false;

    std::optional<AsiInfo> cleo, sampfuncs, moonloader;
    bool hasBass = false;
    std::string bassVersion;
    bool hasLua51 = false;

    std::vector<std::uint8_t> payloadFileExists;  // index-aligned with gen::kFiles
    std::vector<std::uint8_t> payloadFileSame;    // same, 1 = identical to the bundled file (libraries only)
    std::vector<std::uint8_t> payloadDirExists;   // index-aligned with gen::kDirs
    std::vector<LibUnitState> libUnits;

    std::vector<UfScript> ufScripts;
    bool ufExactInstalled = false;  // our UltraFuck_<ver>.luac, byte-identical

    bool d3dx9 = false;
    std::vector<FontState> fonts;

    bool gameRunning = false;       // gta_sa.exe from this folder is running
    bool gameMaybeRunning = false;  // some gta_sa.exe runs, its folder is unknown

    bool arizona = false;
    std::string arizonaId;
    bool arizonaAutoClean = false;

    bool nonAsciiPath = false;
    bool underProgramFiles = false;
    fs::path virtualStoreDir;  // non-empty when VirtualStore holds game files
    bool writable = true;

    std::size_t MissingUnitCount() const;
};

struct InspectOptions {
    std::optional<fs::path> fontsDir;  // default: %WINDIR%\Fonts
    bool simulateMissingFonts = false;
    bool simulateMissingD3dx9 = false;
    const ArizonaInfo* arizona = nullptr;
};

FolderReport Inspect(const fs::path& dir, const InspectOptions& options);
// Human-readable loader name in the current language.
std::string LoaderName(const FolderReport& r);

// Quick checks used by the drive scanner and the installer.
bool HasGtaExe(const fs::path& dir);
GtaVersion DetectGtaVersion(const fs::path& exe);
LoaderKind ClassifyVorbisFile(std::uint64_t size, std::uint32_t crc, bool containsHookedName);
void DetectRunningGame(const fs::path& dir, bool* running, bool* maybe);

}  // namespace uf
