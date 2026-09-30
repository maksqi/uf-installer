#pragma once
// Interface language. Every user-visible string is written in Russian and English next to each other:
//   T("не установлен", "not installed")
//   F("не хватает {} из {}", "{} of {} missing", missing, total)
// Ukrainian is looked up by the Russian text in src/core/i18n_uk.cpp (tools/i18n_check.py checks it is complete).
#include <format>
#include <optional>
#include <string>
#include <string_view>

namespace uf {

enum class Lang { Ru, Uk, En };

Lang CurrentLang();
void SetLang(Lang lang);
// Ukrainian for Ukrainian Windows, Russian for Russian and other CIS languages, English otherwise.
Lang SystemLang();
const char* LangCode(Lang lang);                   // "ru" / "uk" / "en"
std::optional<Lang> ParseLang(std::string_view s);  // "ru" / "uk" (or "ua") / "en"

// Ukrainian text for a Russian one; the Russian text itself if the table has none.
std::string_view Ukrainian(std::string_view ru);

inline const char* T(const char* ru, const char* en) {
    switch (CurrentLang()) {
        case Lang::En: return en;
        case Lang::Uk: return Ukrainian(ru).data();  // table values and `ru` are null-terminated literals
        default: return ru;
    }
}

template <typename... Args>
std::string F(std::string_view ru, std::string_view en, const Args&... args) {
    Lang lang = CurrentLang();
    return std::vformat(lang == Lang::En ? en : lang == Lang::Uk ? Ukrainian(ru) : ru, std::make_format_args(args...));
}

}  // namespace uf
