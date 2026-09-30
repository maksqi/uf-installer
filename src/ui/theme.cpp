#include "ui/theme.h"

#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>

#include <format>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <imgui_internal.h>

#include "core/payload.h"
#include "resource.h"
#include "ui/icons.h"
#include "ui/svg.h"
#include "ui/widgets.h"

namespace uf::ui {

namespace {
Theme g_theme = Theme::Dark;
}  // namespace

Theme CurrentTheme() { return g_theme; }

void SetTheme(Theme theme) {
    g_theme = theme;
    using namespace col;
    if (theme == Theme::Dark) {
        Bg = IM_COL32(12, 12, 12, 255);
        Hover = IM_COL32(20, 20, 20, 255);
        Selected = IM_COL32(26, 26, 26, 255);
        Line = IM_COL32(36, 36, 36, 255);
        Control = IM_COL32(64, 64, 64, 255);
        ControlHover = IM_COL32(110, 110, 110, 255);
        Text = IM_COL32(240, 240, 240, 255);
        TextDim = IM_COL32(172, 172, 172, 255);
        TextFaint = IM_COL32(128, 128, 128, 255);
        Primary = IM_COL32(255, 255, 255, 255);
        PrimaryHover = IM_COL32(225, 225, 225, 255);
        PrimaryActive = IM_COL32(200, 200, 200, 255);
        OnPrimary = IM_COL32(12, 12, 12, 255);
        Warn = IM_COL32(230, 170, 70, 255);
        Err = IM_COL32(236, 92, 92, 255);
    } else {
        Bg = IM_COL32(255, 255, 255, 255);
        Hover = IM_COL32(245, 245, 245, 255);
        Selected = IM_COL32(236, 236, 236, 255);
        Line = IM_COL32(228, 228, 228, 255);
        Control = IM_COL32(188, 188, 188, 255);
        ControlHover = IM_COL32(120, 120, 120, 255);
        Text = IM_COL32(15, 15, 15, 255);
        TextDim = IM_COL32(80, 80, 80, 255);
        TextFaint = IM_COL32(118, 118, 118, 255);
        Primary = IM_COL32(15, 15, 15, 255);
        PrimaryHover = IM_COL32(50, 50, 50, 255);
        PrimaryActive = IM_COL32(80, 80, 80, 255);
        OnPrimary = IM_COL32(255, 255, 255, 255);
        Warn = IM_COL32(170, 100, 0, 255);
        Err = IM_COL32(200, 40, 40, 255);
    }
}

Theme SystemTheme() {
    DWORD value = 0, size = sizeof(value);
    LSTATUS r = RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                             L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &size);
    return r == ERROR_SUCCESS && value != 0 ? Theme::Light : Theme::Dark;
}

const char* ThemeName(Theme theme) { return theme == Theme::Light ? "light" : "dark"; }

std::optional<Theme> ParseTheme(std::string_view s) {
    if (s == "dark") return Theme::Dark;
    if (s == "light") return Theme::Light;
    return std::nullopt;
}

void ApplyTheme(Theme theme, HWND hwnd) {
    SetTheme(theme);
    if (ImGui::GetCurrentContext()) ApplyStyle(UiScale());
    if (hwnd) {
        BOOL dark = theme == Theme::Dark;
        DwmSetWindowAttribute(hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &dark, sizeof(dark));
    }
}

Fonts& GetFonts() {
    static Fonts fonts;
    return fonts;
}

namespace {

ImFont* AddFont(int textId, int iconsId) {
    ImGuiIO& io = ImGui::GetIO();
    auto text = ResourceBytes(textId);
    if (text.empty()) return nullptr;

    // Resource memory lives as long as the process: the atlas must not free it.
    // Plex has a few glyphs in the Private Use Area - leave that range to the icons.
    static const ImWchar kPrivateUse[] = {0xE000, 0xF8FF, 0};
    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false;
    cfg.GlyphExcludeRanges = kPrivateUse;
    cfg.RasterizerMultiply = 1.2f;  // denser glyphs: thin light text on black reads better
    ImFont* font = io.Fonts->AddFontFromMemoryTTF(const_cast<std::uint8_t*>(text.data()), static_cast<int>(text.size()),
                                                  kFontBody, &cfg);
    auto icons = iconsId ? ResourceBytes(iconsId) : std::span<const std::uint8_t>();
    if (font && !icons.empty()) {
        static const ImWchar kIconRanges[] = {ICON_MIN_PH, ICON_MAX_PH, 0};
        ImFontConfig icfg;
        icfg.FontDataOwnedByAtlas = false;
        icfg.MergeMode = true;
        icfg.RasterizerMultiply = 1.1f;
        icfg.GlyphMinAdvanceX = kFontBody * 1.1f;
        icfg.GlyphOffset = ImVec2(0.f, 2.f);  // Phosphor sits on the baseline; center it on the text
        io.Fonts->AddFontFromMemoryTTF(const_cast<std::uint8_t*>(icons.data()), static_cast<int>(icons.size()), kFontBody * 1.1f,
                                       &icfg, kIconRanges);
    }
    return font;
}

ImVec4 V(ImU32 c) { return ImGui::ColorConvertU32ToFloat4(c); }

// Images drawn with ImGui, by key ("logo:24", "flag:ru:22"): a new one only when the DPI changes.
std::map<std::string, std::unique_ptr<ImTextureData>>& TextureCache() {
    static std::map<std::string, std::unique_ptr<ImTextureData>> cache;
    return cache;
}

// Straight-alpha RGBA -> registered ImGui texture: the DX9 backend uploads it, and re-uploads it after a device reset.
std::unique_ptr<ImTextureData> MakeTexture(int w, int h, const std::vector<std::uint8_t>& rgba) {
    auto tex = std::make_unique<ImTextureData>();
    tex->Create(ImTextureFormat_RGBA32, w, h);
    // Pixels use the IM_COL32 packing, which depends on IMGUI_USE_BGRA_PACKED_COLOR.
    auto* dst = static_cast<ImU32*>(tex->GetPixels());
    for (std::size_t i = 0; i < static_cast<std::size_t>(w) * h; ++i)
        dst[i] = IM_COL32(rgba[i * 4], rgba[i * 4 + 1], rgba[i * 4 + 2], rgba[i * 4 + 3]);
    tex->UseColors = true;
    ImGui::RegisterUserTexture(tex.get());
    return tex;
}

// `fill(w, h, rgba)` produces the pixels the first time `key` is asked for.
template <typename Fill>
ImTextureData* CachedTexture(const std::string& key, Fill fill) {
    auto& cache = TextureCache();
    auto it = cache.find(key);
    if (it == cache.end()) {
        int w = 0, h = 0;
        std::vector<std::uint8_t> rgba;
        std::unique_ptr<ImTextureData> tex;
        if (fill(w, h, rgba) && w > 0 && h > 0) tex = MakeTexture(w, h, rgba);
        it = cache.emplace(key, std::move(tex)).first;
    }
    return it->second.get();
}

// The IDI_APP icon at exactly `px` pixels as RGBA; the shell scales it down from the nearest larger size.
bool IconPixels(int px, std::vector<std::uint8_t>& rgba) {
    HICON icon = nullptr;
    if (FAILED(LoadIconWithScaleDown(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APP), px, px, &icon))) return false;
    ICONINFO ii{};
    bool ok = false;
    if (GetIconInfo(icon, &ii)) {
        BITMAPINFO bi{};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = px;
        bi.bmiHeader.biHeight = -px;  // top-down
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        rgba.assign(static_cast<std::size_t>(px) * px * 4, 0);
        HDC dc = GetDC(nullptr);
        ok = ii.hbmColor && GetDIBits(dc, ii.hbmColor, 0, px, rgba.data(), &bi, DIB_RGB_COLORS) == px;
        ReleaseDC(nullptr, dc);
        if (ii.hbmColor) DeleteObject(ii.hbmColor);
        if (ii.hbmMask) DeleteObject(ii.hbmMask);
    }
    DestroyIcon(icon);
    if (!ok) return false;
    // GDI gives BGRA. A low color depth session (e.g. 16-bit RDP) drops the alpha channel: draw the icon opaque then.
    bool hasAlpha = false;
    for (std::size_t i = 0; i < rgba.size(); i += 4) {
        std::swap(rgba[i], rgba[i + 2]);
        hasAlpha |= rgba[i + 3] != 0;
    }
    if (!hasAlpha)
        for (std::size_t i = 3; i < rgba.size(); i += 4) rgba[i] = 255;
    return true;
}

}  // namespace

void LoadFonts() {
    Fonts& f = GetFonts();
    if (f.regular) return;
    f.regular = AddFont(IDR_FONT_UI, IDR_FONT_ICONS);
    f.bold = AddFont(IDR_FONT_UI_BOLD, IDR_FONT_ICONS_BOLD);
    f.mono = AddFont(IDR_FONT_MONO, 0);
    if (!f.regular) f.regular = ImGui::GetIO().Fonts->AddFontDefault();
    if (!f.bold) f.bold = f.regular;
    if (!f.mono) f.mono = f.regular;
    ImGui::GetIO().FontDefault = f.regular;
}

ImTextureData* AppLogo(int px) {
    return CachedTexture(std::format("logo:{}", px), [&](int& w, int& h, std::vector<std::uint8_t>& rgba) {
        w = h = px;
        return px > 0 && IconPixels(px, rgba);
    });
}

ImTextureData* FlagTexture(Lang lang, int width) {
    return CachedTexture(std::format("flag:{}:{}", LangCode(lang), width), [&](int& w, int& h, std::vector<std::uint8_t>& rgba) {
        w = width;
        int id = lang == Lang::Ru ? IDR_FLAG_RU : lang == Lang::Uk ? IDR_FLAG_UA : IDR_FLAG_GB;
        return RasterizeSvgResource(id, width, &h, rgba);
    });
}

void ReleaseTextures() {
    for (auto& [key, tex] : TextureCache())
        if (tex) ImGui::UnregisterUserTexture(tex.get());
    TextureCache().clear();
}

void ApplyStyle(float scale) {
    ImGuiStyle& s = ImGui::GetStyle();
    s = ImGuiStyle();
    s.WindowPadding = ImVec2(0, 0);
    s.WindowBorderSize = 0;
    s.WindowRounding = 0;
    s.ChildBorderSize = 0;
    s.ChildRounding = 0;
    s.FramePadding = ImVec2(12, 8);
    s.FrameRounding = 4;
    s.FrameBorderSize = 0;
    s.ItemSpacing = ImVec2(10, 8);
    s.ItemInnerSpacing = ImVec2(8, 6);
    s.ScrollbarSize = 6;
    s.ScrollbarRounding = 3;
    s.ScrollbarPadding = 0;
    s.GrabRounding = 3;
    s.PopupRounding = 4;
    s.PopupBorderSize = 1;
    s.TabRounding = 4;
    s.SeparatorTextBorderSize = 1;
    s.FontSizeBase = kFontBody;

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text] = V(col::Text);
    c[ImGuiCol_TextDisabled] = V(col::TextFaint);
    c[ImGuiCol_WindowBg] = V(col::Bg);
    c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_PopupBg] = V(col::Hover);
    c[ImGuiCol_Border] = V(col::Line);
    c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg] = V(col::Hover);
    c[ImGuiCol_FrameBgHovered] = V(col::Selected);
    c[ImGuiCol_FrameBgActive] = V(col::Selected);
    c[ImGuiCol_TitleBg] = V(col::Bg);
    c[ImGuiCol_TitleBgActive] = V(col::Bg);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab] = V(col::Line);
    c[ImGuiCol_ScrollbarGrabHovered] = V(col::Control);
    c[ImGuiCol_ScrollbarGrabActive] = V(col::ControlHover);
    c[ImGuiCol_CheckMark] = V(col::Primary);
    c[ImGuiCol_SliderGrab] = V(col::Primary);
    c[ImGuiCol_Button] = V(col::Hover);
    c[ImGuiCol_ButtonHovered] = V(col::Selected);
    c[ImGuiCol_ButtonActive] = V(col::Line);
    c[ImGuiCol_Header] = V(col::Hover);
    c[ImGuiCol_HeaderHovered] = V(col::Selected);
    c[ImGuiCol_HeaderActive] = V(col::Selected);
    c[ImGuiCol_Separator] = V(col::Line);
    c[ImGuiCol_TextSelectedBg] = ImVec4(1.f, 1.f, 1.f, 0.20f);
    c[ImGuiCol_NavCursor] = V(col::ControlHover);

    s.ScaleAllSizes(scale);
    s.FontScaleDpi = scale;
}

}  // namespace uf::ui
