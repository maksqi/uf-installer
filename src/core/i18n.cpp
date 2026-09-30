#include "core/i18n.h"

#include <windows.h>

#include <atomic>

namespace uf {

namespace {
std::atomic<Lang> g_lang{Lang::Ru};
}  // namespace

Lang CurrentLang() { return g_lang.load(std::memory_order_relaxed); }
void SetLang(Lang lang) { g_lang.store(lang, std::memory_order_relaxed); }

Lang SystemLang() {
    LANGID id = GetUserDefaultUILanguage();
    switch (PRIMARYLANGID(id)) {
        case LANG_UKRAINIAN:
            return Lang::Uk;
        case LANG_RUSSIAN:
        case LANG_BELARUSIAN:
        case LANG_KAZAK:
        case LANG_KYRGYZ:
        case LANG_UZBEK:
        case LANG_TAJIK:
        case LANG_TURKMEN:
        case LANG_AZERI:
        case LANG_ARMENIAN:
        case LANG_GEORGIAN:
            return Lang::Ru;
        case LANG_ROMANIAN:
            return SUBLANGID(id) == 0x02 ? Lang::Ru : Lang::En;  // ro-MD (Moldova)
        default:
            return Lang::En;
    }
}

const char* LangCode(Lang lang) { return lang == Lang::En ? "en" : lang == Lang::Uk ? "uk" : "ru"; }

std::optional<Lang> ParseLang(std::string_view s) {
    if (s == "ru") return Lang::Ru;
    if (s == "uk" || s == "ua") return Lang::Uk;
    if (s == "en") return Lang::En;
    return std::nullopt;
}

}  // namespace uf
