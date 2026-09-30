#pragma once
// Types used by the generated payload manifest (payload_manifest.gen.h).
#include <cstdint>

namespace uf {

enum class Comp : std::uint8_t {
    AsiLoader,          // Silent's ASI Loader: vorbisFile.dll, vorbisHooked.dll, scripts\global.ini
    Bass,               // bass.dll (needed by CLEO and MoonLoader)
    Cleo,               // CLEO.asi + cleo\*.cleo
    Sampfuncs,          // SAMPFUNCS.asi
    MoonLoader,         // MoonLoader.asi + lua51.dll
    MoonLoaderScripts,  // default scripts that ship with MoonLoader
    Lib,                // moonloader\lib\** (grouped into units)
    Script,             // moonloader\UltraFuck_<ver>.luac
    Config,             // moonloader\config\UltraFuck\**
};

struct PayloadFile {
    Comp comp;
    const char* entry;   // name inside payload.zip
    const char* dest;    // path relative to the game folder
    std::uint32_t size;
    std::uint32_t crc;
    const char* unit;    // library unit for Comp::Lib, "" otherwise
};

struct PayloadDir {
    Comp comp;
    const char* dest;
};

struct PayloadFont {
    const char* entry;
    const char* file;     // file name in %WINDIR%\Fonts
    const char* regName;  // value name under HKLM\...\Windows NT\CurrentVersion\Fonts
    bool required;        // imgui.lua asserts that this file exists
    std::uint32_t size;
    std::uint32_t crc;
};

struct KnownBinary {
    std::uint32_t size;
    std::uint32_t crc;
};

}  // namespace uf
