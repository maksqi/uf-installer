#include "ui/screens/screens.h"

#include <algorithm>
#include <cmath>

#include "payload_manifest.gen.h"
#include "ui/icons.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace uf::ui {

StateVisual VisualFor(ItemState state) {
    switch (state) {
        case ItemState::Ok: return {col::TextFaint, ICON_CHECK, "в порядке"};
        case ItemState::Install: return {col::Text, ICON_DOWNLOAD, "установить"};
        case ItemState::Update: return {col::Text, ICON_ARROWS_CLOCKWISE, "заменить"};
        case ItemState::Repair: return {col::Text, ICON_WRENCH, "дополнить"};
        case ItemState::Skipped: return {col::TextFaint, ICON_MINUS, "пропустить"};
        case ItemState::Managed: return {col::TextDim, ICON_LOCK, "лаунчер"};
        case ItemState::Warning: return {col::Warn, ICON_WARNING, "внимание"};
        case ItemState::Error: return {col::Err, ICON_WARNING_CIRCLE, "проблема"};
    }
    return {col::TextFaint, ICON_MINUS, ""};
}

StateVisual VisualFor(Severity severity) {
    switch (severity) {
        case Severity::Info: return {col::TextDim, ICON_INFO, ""};
        case Severity::Warning: return {col::Warn, ICON_WARNING, ""};
        case Severity::Error: return {col::Err, ICON_WARNING_CIRCLE, ""};
    }
    return {col::TextDim, ICON_INFO, ""};
}

Layout GetLayout() {
    Layout l;
    l.origin = ImGui::GetWindowPos();
    l.size = ImGui::GetWindowSize();
    l.top = l.origin.y + S(kTitleBarHeight);
    l.margin = S(32);
    return l;
}

bool DrawBanner(Severity severity, const char* text, const char* buttonLabel, float width) {
    StateVisual v = VisualFor(severity);
    Fonts& f = GetFonts();
    if (width <= 0.f) width = ImGui::GetContentRegionAvail().x;
    float buttonW = buttonLabel ? TextSize(f.bold, kFontSmall, buttonLabel).x + S(28) : 0.f;
    float textW = width - S(44) - (buttonLabel ? buttonW + S(16) : S(16));
    ImVec2 ts = TextSize(f.regular, kFontSmall, text, textW);
    float h = std::max(ts.y + S(22), buttonLabel ? S(50) : S(40));
    ImVec2 p = ImGui::GetCursorScreenPos();
    p = ImVec2(Px(p.x), Px(p.y));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + width, p.y + h), col::Hover);
    dl->AddRectFilled(p, ImVec2(p.x + std::max(2.f, Px(S(2))), p.y + h), v.color);
    ImVec2 is = TextSize(f.regular, kFontSmall, v.icon);
    DrawLabel(dl, f.regular, kFontSmall, ImVec2(p.x + S(16), p.y + (h - is.y) * 0.5f), v.color, v.icon);
    DrawLabel(dl, f.regular, kFontSmall, ImVec2(p.x + S(42), p.y + (h - ts.y) * 0.5f), col::Text, text, textW);
    bool pressed = false;
    if (buttonLabel) {
        ImGui::SetCursorScreenPos(ImVec2(p.x + width - buttonW - S(10), p.y + (h - S(32)) * 0.5f));
        pressed = Button(buttonLabel, ImVec2(buttonW, S(32)), ButtonKind::Secondary);
    }
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h + S(8)));
    ImGui::Dummy(ImVec2(width, 0));
    return pressed;
}

float BeginFooter(const Layout& l, float height) {
    float y = Px(l.origin.y + l.size.y - height);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(ImVec2(l.origin.x, y), ImVec2(l.origin.x + l.size.x, l.origin.y + l.size.y), col::Bg);
    HLine(dl, l.origin.x, l.origin.x + l.size.x, y, col::Line);
    return y;
}

namespace {

// "1 Папка — 2 Проверка — 3 Установка": done steps get a check mark, the current one is white.
void DrawSteps(ImDrawList* dl, const App& app, float centerX, float centerY) {
    static const char* kSteps[] = {"Папка", "Проверка", "Установка"};
    int current = app.screen == Screen::Select ? 0 : app.screen == Screen::Analysis ? 1 : 2;
    bool finished = app.screen == Screen::Done && app.result && app.result->ok;
    Fonts& f = GetFonts();
    const float gap = S(10), dash = S(18);

    struct Part {
        std::string mark;
        ImFont* markFont;
        const char* label;
        ImU32 color;
    };
    Part parts[3];
    float total = 0;
    for (int i = 0; i < 3; ++i) {
        bool done = i < current || finished;
        parts[i] = {done ? std::string(ICON_CHECK) : std::to_string(i + 1), done ? f.regular : f.mono, kSteps[i],
                    i == current && !finished ? col::Text : done ? col::TextDim : col::TextFaint};
        total += TextSize(parts[i].markFont, kFontSmall, parts[i].mark.c_str()).x + S(6) + TextSize(f.regular, kFontSmall, kSteps[i]).x;
        if (i < 2) total += gap * 2 + dash;
    }
    float x = centerX - total * 0.5f;
    float th = TextSize(f.regular, kFontSmall, "Ag").y;
    float y = centerY - th * 0.5f;
    for (int i = 0; i < 3; ++i) {
        const Part& part = parts[i];
        DrawLabel(dl, part.markFont, kFontSmall, ImVec2(x, y), part.color, part.mark.c_str());
        x += TextSize(part.markFont, kFontSmall, part.mark.c_str()).x + S(6);
        DrawLabel(dl, f.regular, kFontSmall, ImVec2(x, y), part.color, part.label);
        x += TextSize(f.regular, kFontSmall, part.label).x;
        if (i < 2) {
            HLine(dl, x + gap, x + gap + dash, centerY, col::Line);
            x += gap * 2 + dash;
        }
    }
}

}  // namespace

void DrawTitleBar(App& app) {
    Layout l = GetLayout();
    Fonts& f = GetFonts();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float h = S(kTitleBarHeight);
    ImVec2 a = l.origin, b(l.origin.x + l.size.x, l.origin.y + h);
    dl->AddRectFilled(a, b, col::Bg);
    HLine(dl, a.x, b.x, b.y - S(1), col::Line);

    // App mark: the logo from the exe icon, pixel-aligned so the texture maps 1:1.
    int m = static_cast<int>(S(20) + 0.5f);
    ImVec2 mp(Px(a.x + S(14)), Px(a.y + (h - m) * 0.5f));
    if (ImTextureData* logo = AppLogo(m)) dl->AddImage(logo->GetTexRef(), mp, ImVec2(mp.x + m, mp.y + m));

    float x = mp.x + m + S(10);
    ImVec2 ts = TextSize(f.bold, kFontSmall, "UltraFuck");
    float ty = a.y + (h - ts.y) * 0.5f;
    DrawLabel(dl, f.bold, kFontSmall, ImVec2(x, ty), col::Text, "UltraFuck");
    x += ts.x + S(6);
    DrawLabel(dl, f.regular, kFontSmall, ImVec2(x, ty), col::TextDim, "установщик");
    x += TextSize(f.regular, kFontSmall, "установщик").x + S(8);
    float my = a.y + (h - TextSize(f.mono, kFontMono, "0").y) * 0.5f;
    DrawLabel(dl, f.mono, kFontMono, ImVec2(x, my), col::TextFaint, gen::kScriptVersion);

    DrawSteps(dl, app, a.x + l.size.x * 0.5f, a.y + h * 0.5f);

    // Window buttons.
    float bw = S(kCaptionButtonWidth);
    ImGui::SetCursorScreenPos(ImVec2(b.x - bw * 2, a.y));
    if (IconButton("##min", ICON_MINUS, ImVec2(bw, h - S(1)), col::Hover, col::TextDim, col::Text)) ShowWindow(app.hwnd(), SW_MINIMIZE);
    ImGui::SetCursorScreenPos(ImVec2(b.x - bw, a.y));
    if (IconButton("##close", ICON_X, ImVec2(bw, h - S(1)), col::CloseHover, col::TextDim, col::White))
        PostMessageW(app.hwnd(), WM_CLOSE, 0, 0);
}

}  // namespace uf::ui
