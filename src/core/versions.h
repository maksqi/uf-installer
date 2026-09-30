#pragma once
#include <optional>
#include <string>
#include <string_view>

namespace uf {

// Compares versions written as Lua numbers: "2.4" means 2.40, so 2.4 > 2.36.
// Returns <0, 0, >0. Non-numeric tails ("-fix") are ignored.
int CompareDecimalVersions(std::string_view a, std::string_view b);

// Leading integer of a MoonLoader version ("026.5-beta" -> 26).
int MoonLoaderMajor(std::string_view v);

struct UfScriptName {
    std::string version;  // "" when the file name has no version
    std::string suffix;   // e.g. "-fix"
    std::wstring ext;     // lowercase: lua, luac, aluac
};

// Matches "UltraFuck.lua", "UltraFuck 2.4.lua", "UltraFuck_2.36.luac", "UltraFuck_2.4-fix.aluac"...
std::optional<UfScriptName> ParseUfScriptName(std::wstring_view fileName);

// True when the head of a Lua source declares script_name('UltraFuck').
bool LuaDeclaresUltraFuck(std::string_view head);

}  // namespace uf
