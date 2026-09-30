#include "ui/theme.h"

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
