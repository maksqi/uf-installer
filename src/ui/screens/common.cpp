#include "ui/screens/screens.h"

#include <algorithm>
#include <cmath>

#include "core/i18n.h"
#include "payload_manifest.gen.h"
#include "ui/icons.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace uf::ui {

StateVisual VisualFor(ItemState state) {
    switch (state) {
        case ItemState::Ok: return {col::TextFaint, ICON_CHECK, T("в порядке", "ok")};
        case ItemState::Install: return {col::Text, ICON_DOWNLOAD, T("установить", "install")};
        case ItemState::Update: return {col::Text, ICON_ARROWS_CLOCKWISE, T("заменить", "replace")};
        case ItemState::Repair: return {col::Text, ICON_WRENCH, T("дополнить", "repair")};
        case ItemState::Skipped: return {col::TextFaint, ICON_MINUS, T("пропустить", "skip")};
        case ItemState::Managed: return {col::TextDim, ICON_LOCK, T("лаунчер", "launcher")};
        case ItemState::Warning: return {col::Warn, ICON_WARNING, T("внимание", "warning")};
        case ItemState::Error: return {col::Err, ICON_WARNING_CIRCLE, T("проблема", "problem")};
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
    const char* kSteps[] = {T("Папка", "Folder"), T("Проверка", "Check"), T("Установка", "Install")};
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

// Small title bar button (language / theme) with a tooltip; `image` (a flag) goes left of the text.
bool TitleSwitch(const char* id, const char* text, ImFont* font, float size, ImVec2 box, const char* tooltip,
                 ImTextureData* image = nullptr) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool pressed = ImGui::InvisibleButton(id, box);
    bool hovered = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (hovered) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        dl->AddRectFilled(p, ImVec2(p.x + box.x, p.y + box.y), col::Hover);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(S(10), S(6)));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, S(4));
        ImGui::PushFont(GetFonts().regular, kFontTiny);
        ImGui::SetTooltip("%s", tooltip);
        ImGui::PopFont();
        ImGui::PopStyleVar(2);
    }
    ImVec2 ts = TextSize(font, size, text);
    ImU32 color = hovered ? col::Text : col::TextDim;
    float tx = p.x + (box.x - ts.x) * 0.5f;
    if (image) {
        // Flag and code centered together; the texture has the exact pixel size, so it maps 1:1.
        const float gap = S(7);
        float iw = static_cast<float>(image->Width), ih = static_cast<float>(image->Height);
        ImVec2 ip(Px(p.x + (box.x - iw - gap - ts.x) * 0.5f), Px(p.y + (box.y - ih) * 0.5f));
        ImVec2 iq(ip.x + iw, ip.y + ih);
        dl->AddImageRounded(image->GetTexRef(), ip, iq, ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, S(2));
        // Hairline so the white stripe does not melt into the light theme.
        dl->AddRect(ImVec2(ip.x + 0.5f, ip.y + 0.5f), ImVec2(iq.x - 0.5f, iq.y - 0.5f), WithAlpha(col::Text, 0.18f), S(2), 1.f);
        tx = iq.x + gap;
    }
    DrawLabel(dl, font, size, ImVec2(tx, p.y + (box.y - ts.y) * 0.5f), color, text);
    return pressed;
}

constexpr Lang kLanguages[] = {Lang::Ru, Lang::Uk, Lang::En};
const char* LangLabel(Lang lang) { return lang == Lang::Ru ? "RU" : lang == Lang::Uk ? "UA" : "EN"; }
const char* LangName(Lang lang) { return lang == Lang::Ru ? "Русский" : lang == Lang::Uk ? "Українська" : "English"; }
int FlagWidth() { return std::max(4, static_cast<int>(S(20) / 4.f + 0.5f) * 4); }  // multiple of 4: exact 4:3 in pixels

// Drop-down under the language button: flag + name of every language, a check mark at the current one.
void LanguageMenu(App& app, ImVec2 topRight) {
    ImGui::SetNextWindowPos(topRight, ImGuiCond_Always, ImVec2(1.f, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(S(4), S(4)));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, S(4));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, col::Bg);
    ImGui::PushStyleColor(ImGuiCol_Border, col::Control);
    if (ImGui::BeginPopup("##langmenu")) {
        Fonts& f = GetFonts();
        const ImVec2 row(S(184), S(38));
        const int flagW = FlagWidth();
        for (Lang lang : kLanguages) {
            ImGui::PushID(static_cast<int>(lang));
            ImVec2 p = ImGui::GetCursorScreenPos();
            bool pressed = ImGui::InvisibleButton("##item", row);
            bool hovered = ImGui::IsItemHovered();
            ImGui::PopID();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            if (hovered) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                dl->AddRectFilled(p, ImVec2(p.x + row.x, p.y + row.y), col::Hover, S(3));
            }
            float x = p.x + S(10);
            if (ImTextureData* flag = FlagTexture(lang, flagW)) {
                ImVec2 ip(Px(x), Px(p.y + (row.y - flag->Height) * 0.5f));
                ImVec2 iq(ip.x + flag->Width, ip.y + flag->Height);
                dl->AddImageRounded(flag->GetTexRef(), ip, iq, ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, S(2));
                dl->AddRect(ImVec2(ip.x + 0.5f, ip.y + 0.5f), ImVec2(iq.x - 0.5f, iq.y - 0.5f), WithAlpha(col::Text, 0.18f), S(2), 1.f);
                x = iq.x + S(10);
            }
            bool current = lang == CurrentLang();
            ImVec2 ts = TextSize(f.regular, kFontSmall, LangName(lang));
            DrawLabel(dl, current ? f.bold : f.regular, kFontSmall, ImVec2(x, p.y + (row.y - ts.y) * 0.5f), col::Text, LangName(lang));
            if (current) {
                ImVec2 cs = TextSize(f.regular, kFontSmall, ICON_CHECK);
                DrawLabel(dl, f.regular, kFontSmall, ImVec2(p.x + row.x - S(10) - cs.x, p.y + (row.y - cs.y) * 0.5f), col::Text, ICON_CHECK);
            }
            if (pressed) {
                app.SetLanguage(lang);
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(4);
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
    const char* setup = T("установщик", "setup");
    DrawLabel(dl, f.regular, kFontSmall, ImVec2(x, ty), col::TextDim, setup);
    x += TextSize(f.regular, kFontSmall, setup).x + S(8);
    float my = a.y + (h - TextSize(f.mono, kFontMono, "0").y) * 0.5f;
    DrawLabel(dl, f.mono, kFontMono, ImVec2(x, my), col::TextFaint, gen::kScriptVersion);

    DrawSteps(dl, app, a.x + l.size.x * 0.5f, a.y + h * 0.5f);

    // Language and theme switches, left of the window buttons.
    float bw = S(kCaptionButtonWidth);
    float tx = b.x - bw * 2 - S(kTitleToolsWidth);
    const float langW = S(64), themeW = S(kTitleToolsWidth) - langW;
    ImGui::SetCursorScreenPos(ImVec2(tx, a.y));
    if (TitleSwitch("##lang", LangLabel(CurrentLang()), f.bold, kFontTiny, ImVec2(langW, h - S(1)), "Язык · Мова · Language",
                    FlagTexture(CurrentLang(), FlagWidth())))
        ImGui::OpenPopup("##langmenu");
    LanguageMenu(app, ImVec2(tx + langW, a.y + h + S(4)));
    ImGui::SetCursorScreenPos(ImVec2(tx + langW, a.y));
    bool dark = CurrentTheme() == Theme::Dark;
    if (TitleSwitch("##theme", dark ? ICON_SUN : ICON_MOON, f.regular, kFontBody, ImVec2(themeW, h - S(1)),
                    dark ? T("Светлая тема", "Light theme") : T("Тёмная тема", "Dark theme")))
        app.ToggleTheme();

    // Window buttons.
    ImGui::SetCursorScreenPos(ImVec2(b.x - bw * 2, a.y));
    if (IconButton("##min", ICON_MINUS, ImVec2(bw, h - S(1)), col::Hover, col::TextDim, col::Text)) ShowWindow(app.hwnd(), SW_MINIMIZE);
    ImGui::SetCursorScreenPos(ImVec2(b.x - bw, a.y));
    if (IconButton("##close", ICON_X, ImVec2(bw, h - S(1)), col::CloseHover, col::TextDim, IM_COL32_WHITE))
        PostMessageW(app.hwnd(), WM_CLOSE, 0, 0);
}

}  // namespace uf::ui
