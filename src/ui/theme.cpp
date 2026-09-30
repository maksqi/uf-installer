#include "ui/theme.h"

#include <windows.h>
#include <commctrl.h>

#include <map>
#include <memory>
#include <vector>

#include <imgui_internal.h>

#include "core/payload.h"
#include "resource.h"
#include "ui/IconsFontAwesome6.h"

namespace uf::ui {

Fonts& GetFonts() {
    static Fonts fonts;
    return fonts;
}

namespace {

ImFont* AddFont(int textId) {
    ImGuiIO& io = ImGui::GetIO();
    auto text = ResourceBytes(textId);
    auto icons = ResourceBytes(IDR_FONT_ICONS);
    if (text.empty()) return nullptr;

    // Resource memory lives as long as the process: the atlas must not free it.
    // Inter has its own glyphs in the Private Use Area - leave that range to Font Awesome.
    static const ImWchar kPrivateUse[] = {0xE000, 0xF8FF, 0};
    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false;
    cfg.GlyphExcludeRanges = kPrivateUse;
    ImFont* font = io.Fonts->AddFontFromMemoryTTF(const_cast<std::uint8_t*>(text.data()), static_cast<int>(text.size()),
                                                  kFontBody, &cfg);
    if (font && !icons.empty()) {
        static const ImWchar kIconRanges[] = {ICON_MIN_FA, ICON_MAX_16_FA, 0};
        ImFontConfig icfg;
        icfg.FontDataOwnedByAtlas = false;
        icfg.MergeMode = true;
        icfg.GlyphMinAdvanceX = kFontBody * 1.15f;
        icfg.GlyphOffset = ImVec2(0.f, 0.5f);
        io.Fonts->AddFontFromMemoryTTF(const_cast<std::uint8_t*>(icons.data()), static_cast<int>(icons.size()), kFontBody * 0.9f,
                                       &icfg, kIconRanges);
    }
    return font;
}

ImVec4 V(ImU32 c) { return ImGui::ColorConvertU32ToFloat4(c); }

// One texture per pixel size (a new one only when the DPI changes).
std::map<int, std::unique_ptr<ImTextureData>>& LogoCache() {
    static std::map<int, std::unique_ptr<ImTextureData>> cache;
    return cache;
}

// The IDI_APP icon at exactly `px` pixels as BGRA; the shell scales it down from the nearest larger size.
bool IconPixels(int px, std::vector<std::uint8_t>& bgra) {
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
        bgra.assign(static_cast<std::size_t>(px) * px * 4, 0);
        HDC dc = GetDC(nullptr);
        ok = ii.hbmColor && GetDIBits(dc, ii.hbmColor, 0, px, bgra.data(), &bi, DIB_RGB_COLORS) == px;
        ReleaseDC(nullptr, dc);
        if (ii.hbmColor) DeleteObject(ii.hbmColor);
        if (ii.hbmMask) DeleteObject(ii.hbmMask);
    }
    DestroyIcon(icon);
    if (!ok) return false;
    // A low color depth session (e.g. 16-bit RDP) drops the alpha channel: draw the icon opaque then.
    bool hasAlpha = false;
    for (std::size_t i = 3; i < bgra.size() && !hasAlpha; i += 4) hasAlpha = bgra[i] != 0;
    if (!hasAlpha)
        for (std::size_t i = 3; i < bgra.size(); i += 4) bgra[i] = 255;
    return true;
}

}  // namespace

void LoadFonts() {
    Fonts& f = GetFonts();
    if (f.regular) return;
    f.regular = AddFont(IDR_FONT_UI);
    f.bold = AddFont(IDR_FONT_UI_BOLD);
    if (!f.regular) f.regular = ImGui::GetIO().Fonts->AddFontDefault();
    if (!f.bold) f.bold = f.regular;
    ImGui::GetIO().FontDefault = f.regular;
}

ImTextureData* AppLogo(int px) {
    auto& cache = LogoCache();
    auto it = cache.find(px);
    if (it == cache.end()) {
        std::unique_ptr<ImTextureData> tex;
        std::vector<std::uint8_t> bgra;
        if (px > 0 && IconPixels(px, bgra)) {
            // The DX9 backend uploads it, and re-uploads it after a device reset.
            // Pixels use the IM_COL32 packing, which depends on IMGUI_USE_BGRA_PACKED_COLOR.
            tex = std::make_unique<ImTextureData>();
            tex->Create(ImTextureFormat_RGBA32, px, px);
            auto* dst = static_cast<ImU32*>(tex->GetPixels());
            for (std::size_t i = 0; i < bgra.size() / 4; ++i)
                dst[i] = IM_COL32(bgra[i * 4 + 2], bgra[i * 4 + 1], bgra[i * 4], bgra[i * 4 + 3]);
            tex->UseColors = true;
            ImGui::RegisterUserTexture(tex.get());
        }
        it = cache.emplace(px, std::move(tex)).first;
    }
    return it->second.get();
}

void ReleaseAppLogos() {
    for (auto& [px, tex] : LogoCache())
        if (tex) ImGui::UnregisterUserTexture(tex.get());
    LogoCache().clear();
}

void ApplyStyle(float scale) {
    ImGuiStyle& s = ImGui::GetStyle();
    s = ImGuiStyle();
    s.WindowPadding = ImVec2(0, 0);
    s.WindowBorderSize = 0;
    s.WindowRounding = 0;
    s.ChildBorderSize = 0;
    s.ChildRounding = 10;
    s.FramePadding = ImVec2(12, 8);
    s.FrameRounding = 8;
    s.FrameBorderSize = 0;
    s.ItemSpacing = ImVec2(10, 8);
    s.ItemInnerSpacing = ImVec2(8, 6);
    s.ScrollbarSize = 10;
    s.ScrollbarRounding = 8;
    s.GrabRounding = 8;
    s.PopupRounding = 8;
    s.PopupBorderSize = 1;
    s.TabRounding = 8;
    s.SeparatorTextBorderSize = 1;
    s.FontSizeBase = kFontBody;

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text] = V(col::Text);
    c[ImGuiCol_TextDisabled] = V(col::TextFaint);
    c[ImGuiCol_WindowBg] = V(col::Bg);
    c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_PopupBg] = V(col::Surface);
    c[ImGuiCol_Border] = V(col::Border);
    c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg] = V(col::SurfaceHover);
    c[ImGuiCol_FrameBgHovered] = V(col::SurfaceActive);
    c[ImGuiCol_FrameBgActive] = V(col::SurfaceActive);
    c[ImGuiCol_TitleBg] = V(col::TitleBar);
    c[ImGuiCol_TitleBgActive] = V(col::TitleBar);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab] = V(col::Border);
    c[ImGuiCol_ScrollbarGrabHovered] = V(col::Muted);
    c[ImGuiCol_ScrollbarGrabActive] = V(col::TextFaint);
    c[ImGuiCol_CheckMark] = V(col::Accent);
    c[ImGuiCol_SliderGrab] = V(col::Accent);
    c[ImGuiCol_Button] = V(col::SurfaceHover);
    c[ImGuiCol_ButtonHovered] = V(col::SurfaceActive);
    c[ImGuiCol_ButtonActive] = V(col::Border);
    c[ImGuiCol_Header] = V(col::SurfaceHover);
    c[ImGuiCol_HeaderHovered] = V(col::SurfaceActive);
    c[ImGuiCol_HeaderActive] = V(col::SurfaceActive);
    c[ImGuiCol_Separator] = V(col::Border);
    c[ImGuiCol_TextSelectedBg] = ImVec4(0.545f, 0.361f, 0.965f, 0.35f);
    c[ImGuiCol_NavCursor] = V(col::AccentHover);

    s.ScaleAllSizes(scale);
    s.FontScaleDpi = scale;
}

}  // namespace uf::ui
