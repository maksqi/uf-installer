#include "ui/screens/screens.h"

#include <algorithm>

#include "payload_manifest.gen.h"
#include "ui/IconsFontAwesome6.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace uf::ui {

StateVisual VisualFor(ItemState state) {
    switch (state) {
        case ItemState::Ok: return {col::Ok, ICON_FA_CHECK, "OK"};
        case ItemState::Install: return {col::Accent, ICON_FA_DOWNLOAD, "Установить"};
        case ItemState::Update: return {col::Info, ICON_FA_ARROWS_ROTATE, "Заменить"};
        case ItemState::Repair: return {col::Accent, ICON_FA_WRENCH, "Дополнить"};
        case ItemState::Skipped: return {col::Muted, ICON_FA_MINUS, "Пропустить"};
        case ItemState::Managed: return {col::Muted, ICON_FA_LOCK, "Лаунчер"};
        case ItemState::Warning: return {col::Warn, ICON_FA_TRIANGLE_EXCLAMATION, "Внимание"};
        case ItemState::Error: return {col::Err, ICON_FA_XMARK, "Проблема"};
    }
    return {col::Muted, ICON_FA_MINUS, ""};
}

StateVisual VisualFor(Severity severity) {
    switch (severity) {
        case Severity::Info: return {col::Info, ICON_FA_CIRCLE_INFO, ""};
        case Severity::Warning: return {col::Warn, ICON_FA_TRIANGLE_EXCLAMATION, ""};
        case Severity::Error: return {col::Err, ICON_FA_CIRCLE_EXCLAMATION, ""};
    }
    return {col::Info, ICON_FA_CIRCLE_INFO, ""};
}

Layout GetLayout() {
    Layout l;
    l.origin = ImGui::GetWindowPos();
    l.size = ImGui::GetWindowSize();
    l.top = l.origin.y + S(kTitleBarHeight);
    l.margin = S(28);
    return l;
}

bool DrawBanner(Severity severity, const char* text, const char* buttonLabel, float width) {
    StateVisual v = VisualFor(severity);
    Fonts& f = GetFonts();
    if (width <= 0.f) width = ImGui::GetContentRegionAvail().x;
    float buttonW = buttonLabel ? TextSize(f.bold, kFontBody, buttonLabel).x + S(28) : 0.f;
    float textW = width - S(44) - (buttonLabel ? buttonW + S(12) : S(12));
    ImVec2 ts = TextSize(f.regular, kFontSmall, text, textW);
    float h = std::max(ts.y + S(20), buttonLabel ? S(52) : S(36));
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + width, p.y + h), WithAlpha(v.color, 0.10f), S(10));
    dl->AddRect(p, ImVec2(p.x + width, p.y + h), WithAlpha(v.color, 0.30f), S(10), S(1));
    ImVec2 is = TextSize(f.regular, kFontBody, v.icon);
    DrawLabel(dl, f.regular, kFontBody, ImVec2(p.x + S(14), p.y + (h - is.y) * 0.5f), v.color, v.icon);
    DrawLabel(dl, f.regular, kFontSmall, ImVec2(p.x + S(40), p.y + (h - ts.y) * 0.5f), col::Text, text, textW);
    bool pressed = false;
    if (buttonLabel) {
        ImGui::SetCursorScreenPos(ImVec2(p.x + width - buttonW - S(10), p.y + (h - S(34)) * 0.5f));
        pressed = Button(buttonLabel, ImVec2(buttonW, S(34)), ButtonKind::Secondary);
    }
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h + S(8)));
    ImGui::Dummy(ImVec2(width, 0));
    return pressed;
}

float BeginFooter(const Layout& l, float height) {
    float y = l.origin.y + l.size.y - height;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(ImVec2(l.origin.x, y), ImVec2(l.origin.x + l.size.x, l.origin.y + l.size.y), col::TitleBar);
    dl->AddLine(ImVec2(l.origin.x, y), ImVec2(l.origin.x + l.size.x, y), col::Border, S(1));
    return y;
}

void DrawTitleBar(App& app) {
    Layout l = GetLayout();
    Fonts& f = GetFonts();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float h = S(kTitleBarHeight);
    ImVec2 a = l.origin, b(l.origin.x + l.size.x, l.origin.y + h);
    dl->AddRectFilled(a, b, col::TitleBar);
    dl->AddLine(ImVec2(a.x, b.y - S(0.5f)), ImVec2(b.x, b.y - S(0.5f)), col::Border, S(1));

    // App mark: rounded violet square with "UF".
    float m = S(24);
    ImVec2 mp(a.x + S(14), a.y + (h - m) * 0.5f);
    dl->AddRectFilled(mp, ImVec2(mp.x + m, mp.y + m), col::Accent, S(6));
    dl->AddRectFilled(ImVec2(mp.x, mp.y + m * 0.5f), ImVec2(mp.x + m, mp.y + m), WithAlpha(col::Accent2, 0.55f), S(6),
                      ImDrawFlags_RoundCornersBottom);
    ImVec2 us = TextSize(f.bold, 11.f, "UF");
    DrawLabel(dl, f.bold, 11.f, ImVec2(mp.x + (m - us.x) * 0.5f, mp.y + (m - us.y) * 0.5f), IM_COL32_WHITE, "UF");

    float x = mp.x + m + S(10);
    ImVec2 ts = TextSize(f.bold, kFontBody, "UltraFuck");
    float ty = a.y + (h - ts.y) * 0.5f;
    DrawLabel(dl, f.bold, kFontBody, ImVec2(x, ty), col::Text, "UltraFuck");
    x += ts.x + S(8);
    DrawLabel(dl, f.regular, kFontBody, ImVec2(x, ty), col::TextDim, "установщик");
    x += TextSize(f.regular, kFontBody, "установщик").x + S(10);
    std::string ver = std::string("v") + gen::kScriptVersion;
    Badge(dl, ImVec2(x, a.y + (h - S(21)) * 0.5f), ver.c_str(), col::Accent);

    // Window buttons.
    float bw = S(kTitleBarHeight);
    ImGui::SetCursorScreenPos(ImVec2(b.x - bw * 2, a.y));
    if (IconButton("##min", ICON_FA_MINUS, ImVec2(bw, h), col::SurfaceHover, col::TextDim, col::Text))
        ShowWindow(app.hwnd(), SW_MINIMIZE);
    ImGui::SetCursorScreenPos(ImVec2(b.x - bw, a.y));
    if (IconButton("##close", ICON_FA_XMARK, ImVec2(bw, h), col::Err, col::TextDim, IM_COL32_WHITE))
        PostMessageW(app.hwnd(), WM_CLOSE, 0, 0);
}

}  // namespace uf::ui
