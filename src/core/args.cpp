#include "core/args.h"

#include <cwchar>
#include <format>

#include "core/elevation.h"

namespace uf {

Args ParseArgs(int argc, wchar_t** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        std::wstring s = argv[i];
        auto next = [&]() -> std::optional<std::wstring> {
            if (i + 1 < argc) return std::wstring(argv[++i]);
            return std::nullopt;
        };
        auto nextInt = [&](int def) {
            auto v = next();
            return v ? static_cast<int>(std::wcstol(v->c_str(), nullptr, 10)) : def;
        };
        if (s == L"--target") {
            if (auto v = next()) a.target = fs::path(*v);
        } else if (s == L"--install") {
            a.install = true;
        } else if (s == L"--elevated") {
            a.elevated = true;
        } else if (s == L"--no-elevate") {
            a.noElevate = true;
        } else if (s == L"--opts") {
            if (auto v = next()) a.opts = ToUtf8(*v);
        } else if (s == L"--plan-hash") {
            if (auto v = next()) a.planHash = std::wcstoull(v->c_str(), nullptr, 16);
        } else if (s == L"--fonts-dir") {
            if (auto v = next()) a.fontsDir = fs::path(*v);
        } else if (s == L"--simulate-missing-fonts") {
            a.simulateMissingFonts = true;
        } else if (s == L"--simulate-missing-d3dx9") {
            a.simulateMissingD3dx9 = true;
        } else if (s == L"--arizona-root") {
            if (auto v = next()) a.arizonaRoot = fs::path(*v);
        } else if (s == L"--arizona-settings") {
            if (auto v = next()) a.arizonaSettings = fs::path(*v);
        } else if (s == L"--no-scan") {
            a.noScan = true;
        } else if (s == L"--screenshot") {
            if (auto v = next()) a.screenshot = fs::path(*v);
        } else if (s == L"--screenshot-delay") {
            a.screenshotDelayMs = nextInt(a.screenshotDelayMs);
        } else if (s == L"--dpi-scale") {
            if (auto v = next()) a.dpiScale = static_cast<float>(std::wcstod(v->c_str(), nullptr));
        } else if (s == L"--throttle-ms") {
            a.throttleMs = nextInt(0);
        } else if (s == L"--fail-after") {
            a.failAfter = nextInt(-1);
        } else if (s == L"--pos") {
            if (auto v = next()) {
                int x = 0, y = 0;
                if (swscanf_s(v->c_str(), L"%d,%d", &x, &y) == 2) a.pos = {x, y};
            }
        } else if (s == L"--lang") {
            if (auto v = next()) a.lang = ToUtf8(*v);
        } else if (s == L"--theme") {
            if (auto v = next()) a.theme = ToUtf8(*v);
        } else if (s == L"--json") {
            a.json = true;
        } else if (s == L"--yes" || s == L"-y") {
            a.yes = true;
        } else if (s.starts_with(L"--")) {
            a.unknown.push_back(s);
        } else {
            a.positional.push_back(s);
        }
    }
    return a;
}

std::wstring BuildElevatedParameters(const Args& base, const fs::path& target, const std::string& opts, std::uint64_t planHash,
                                     bool install, std::optional<std::pair<int, int>> pos) {
    std::wstring p = L"--elevated --target " + QuoteArg(ToUncIfMapped(target).native());
    if (!opts.empty()) p += L" --opts " + QuoteArg(ToWide(opts));
    p += std::format(L" --plan-hash {:016x}", planHash);
    if (install) p += L" --install";
    if (pos) p += std::format(L" --pos {},{}", pos->first, pos->second);
    if (base.fontsDir) p += L" --fonts-dir " + QuoteArg(base.fontsDir->native());
    if (base.simulateMissingFonts) p += L" --simulate-missing-fonts";
    if (base.simulateMissingD3dx9) p += L" --simulate-missing-d3dx9";
    if (base.arizonaRoot) p += L" --arizona-root " + QuoteArg(base.arizonaRoot->native());
    if (base.arizonaSettings) p += L" --arizona-settings " + QuoteArg(base.arizonaSettings->native());
    if (base.throttleMs > 0) p += std::format(L" --throttle-ms {}", base.throttleMs);
    if (base.dpiScale > 0) p += std::format(L" --dpi-scale {}", base.dpiScale);
    if (!base.lang.empty()) p += L" --lang " + QuoteArg(ToWide(base.lang));
    if (!base.theme.empty()) p += L" --theme " + QuoteArg(ToWide(base.theme));
    return p;
}

}  // namespace uf
