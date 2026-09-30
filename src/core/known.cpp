#include "core/known.h"
#include "core/i18n.h"

#include <algorithm>
#include <regex>
#include <string>

namespace uf {

SampVersion SampVersionFromEntryPoint(std::uint32_t entryRva) {
    for (const auto& e : kSampEntryPoints)
        if (e.entryRva == entryRva) return e.version;
    return SampVersion::Unknown;
}

std::string_view SampVersionName(SampVersion v) {
    switch (v) {
        case SampVersion::None: return T("не установлен", "not installed");
        case SampVersion::R1: return "0.3.7-R1";
        case SampVersion::R2: return "0.3.7-R2";
        case SampVersion::R3_1: return "0.3.7-R3-1";
        case SampVersion::R4: return "0.3.7-R4";
        case SampVersion::R5: return "0.3.7-R5";
        case SampVersion::DL: return "0.3.DL-R1";
        case SampVersion::Unknown: return T("неизвестная сборка", "unknown build");
    }
    return "?";
}

std::string_view SampShortName(SampVersion v) {
    switch (v) {
        case SampVersion::R1: return "R1";
        case SampVersion::R2: return "R2";
        case SampVersion::R3_1: return "R3-1";
        case SampVersion::R4: return "R4";
        case SampVersion::R5: return "R5";
        case SampVersion::DL: return "DL";
        default: return "?";
    }
}

std::string_view GtaVersionName(GtaVersion v) {
    switch (v) {
        case GtaVersion::Missing: return T("не найдена", "not found");
        case GtaVersion::US10: return "1.0 US";
        case GtaVersion::EU10: return "1.0 EU";
        case GtaVersion::US101: return "1.01 US";
        case GtaVersion::EU101: return "1.01 EU";
        case GtaVersion::V3: return "3.0 (Steam)";
        case GtaVersion::Unknown: return T("неизвестная версия", "unknown version");
    }
    return "?";
}

std::vector<SampVersion> SampVersionsFromText(std::string_view text) {
    std::string s(text);
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    std::vector<SampVersion> out;
    auto add = [&](SampVersion v) {
        if (std::find(out.begin(), out.end(), v) == out.end()) out.push_back(v);
    };
    static const std::regex re(R"((DL)|R(\d)(?:-(\d))?)");
    for (auto it = std::sregex_iterator(s.begin(), s.end(), re); it != std::sregex_iterator(); ++it) {
        const auto& m = *it;
        if (m[1].matched) {
            add(SampVersion::DL);
            continue;
        }
        switch (m[2].str()[0]) {
            case '1': add(SampVersion::R1); break;
            case '2': add(SampVersion::R2); break;
            case '3': add(SampVersion::R3_1); break;
            case '4': add(SampVersion::R4); break;
            case '5': add(SampVersion::R5); break;
            default: break;
        }
    }
    return out;
}

}  // namespace uf
