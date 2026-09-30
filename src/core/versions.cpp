#include "core/versions.h"

#include <algorithm>
#include <cctype>
#include <regex>

#include "core/paths.h"

namespace uf {

namespace {

struct Decimal {
    std::string whole;
    std::string fraction;
    std::string rest;  // further dotted components, compared numerically
};

Decimal SplitDecimal(std::string_view v) {
    Decimal d;
    std::size_t i = 0;
    while (i < v.size() && std::isdigit(static_cast<unsigned char>(v[i]))) d.whole.push_back(v[i++]);
    if (i < v.size() && v[i] == '.') {
        ++i;
        while (i < v.size() && std::isdigit(static_cast<unsigned char>(v[i]))) d.fraction.push_back(v[i++]);
    }
    while (i < v.size() && (std::isdigit(static_cast<unsigned char>(v[i])) || v[i] == '.')) d.rest.push_back(v[i++]);
    return d;
}

int CompareIntStrings(std::string a, std::string b) {
    auto strip = [](std::string& s) {
        s.erase(0, std::min(s.find_first_not_of('0'), s.size()));
    };
    strip(a);
    strip(b);
    if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
    return a.compare(b) < 0 ? -1 : (a == b ? 0 : 1);
}

}  // namespace

int CompareDecimalVersions(std::string_view a, std::string_view b) {
    Decimal x = SplitDecimal(a), y = SplitDecimal(b);
    if (int c = CompareIntStrings(x.whole, y.whole)) return c;
    std::size_t n = std::max(x.fraction.size(), y.fraction.size());
    x.fraction.resize(n, '0');
    y.fraction.resize(n, '0');
    if (int c = x.fraction.compare(y.fraction)) return c < 0 ? -1 : 1;
    // Remaining ".x.y" parts: compare component-wise as integers.
    auto next = [](std::string& s) {
        if (!s.empty() && s[0] == '.') s.erase(0, 1);
        std::size_t dot = s.find('.');
        std::string part = s.substr(0, dot);
        s = dot == std::string::npos ? "" : s.substr(dot);
        return part;
    };
    while (!x.rest.empty() || !y.rest.empty()) {
        if (int c = CompareIntStrings(next(x.rest), next(y.rest))) return c;
    }
    return 0;
}

int MoonLoaderMajor(std::string_view v) {
    int n = 0;
    bool any = false;
    for (char c : v) {
        if (!std::isdigit(static_cast<unsigned char>(c))) break;
        n = n * 10 + (c - '0');
        any = true;
    }
    return any ? n : -1;
}

std::optional<UfScriptName> ParseUfScriptName(std::wstring_view fileName) {
    static const std::wregex re(LR"(^ultrafuck(?:[ _-]?v?(\d+(?:\.\d+)*)(.*?))?\.(lua|luac|aluac)$)", std::regex::icase);
    std::wstring name(fileName);
    std::wsmatch m;
    if (!std::regex_match(name, m, re)) return std::nullopt;
    UfScriptName r;
    r.version = ToUtf8(m[1].str());
    r.suffix = ToUtf8(m[2].str());
    r.ext = ToLower(m[3].str());
    return r;
}

bool LuaDeclaresUltraFuck(std::string_view head) {
    std::string lower(head);
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    for (std::size_t pos = lower.find("script_name"); pos != std::string::npos; pos = lower.find("script_name", pos + 1)) {
        std::string_view window = std::string_view(lower).substr(pos, 40);
        if (window.find("ultrafuck") != std::string_view::npos) return true;
    }
    return false;
}

}  // namespace uf
