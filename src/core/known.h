#pragma once
// Well-known constants of GTA San Andreas / SA-MP builds.
#include <cstdint>
#include <string_view>
#include <vector>

namespace uf {

enum class SampVersion { None, R1, R2, R3_1, R4, R5, DL, Unknown };
enum class GtaVersion { Missing, US10, EU10, US101, EU101, V3, Unknown };

struct SampEntryPoint {
    std::uint32_t entryRva;
    SampVersion version;
};

// samp.dll AddressOfEntryPoint per client build (R4 and R4-2 share one).
inline constexpr SampEntryPoint kSampEntryPoints[] = {
    {0x31DF13, SampVersion::R1},   {0x3195DD, SampVersion::R2}, {0xCC4D0, SampVersion::R3_1},
    {0xCBCB0, SampVersion::R4},    {0xCBC90, SampVersion::R5},  {0xFDB60, SampVersion::DL},
};

// The same check CLEO/plugin-sdk use at runtime: DWORD 0x94BF at a version-specific VA.
struct GtaSignature {
    std::uint32_t va;
    GtaVersion version;
};
inline constexpr std::uint32_t kGtaSignatureValue = 0x94BF;
inline constexpr GtaSignature kGtaSignatures[] = {
    {0x82457C, GtaVersion::US10}, {0x8245BC, GtaVersion::EU10}, {0x8252FC, GtaVersion::US101},
    {0x82533C, GtaVersion::EU101}, {0x85EC4A, GtaVersion::V3},
};

SampVersion SampVersionFromEntryPoint(std::uint32_t entryRva);
std::string_view SampVersionName(SampVersion v);
std::string_view SampShortName(SampVersion v);  // "R1", "R3-1", "DL"
std::string_view GtaVersionName(GtaVersion v);
// Parses the SA-MP build(s) from a SAMPFUNCS banner such as "0.3.7-R1" or "0.3.7 R3-1".
std::vector<SampVersion> SampVersionsFromText(std::string_view text);

}  // namespace uf
