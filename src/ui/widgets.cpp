#include "ui/widgets.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>

#include "ui/theme.h"

namespace uf::ui {

namespace {
float g_scale = 1.f;

const char* LabelEnd(const char* label) {
    const char* hash = std::strstr(label, "##");
    return hash ? hash : label + std::strlen(label);
}

float Thin() { return std::max(1.f, Px(S(1))); }
}  // namespace

void SetUiScale(float scale) { g_scale = scale; }
float UiScale() { return g_scale; }

ImU32 WithAlpha(ImU32 c, float alpha) {
    ImVec4 v = ImGui::ColorConvertU32ToFloat4(c);
    v.w *= alpha;
    return ImGui::ColorConvertFloat4ToU32(v);
}

ImVec2 TextSize(ImFont* font, float size, const char* text, float wrapWidth) {
    return font->CalcTextSizeA(S(size), FLT_MAX, wrapWidth, text);
}

void DrawLabel(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, ImU32 color, const char* text, float wrapWidth) {
    dl->AddText(font, S(size), ImVec2(Px(pos.x), Px(pos.y)), color, text, nullptr, wrapWidth);
}

std::string EllipsizeLeft(ImFont* font, float size, const std::string& text, float maxWidth) {
    if (TextSize(font, size, text.c_str()).x <= maxWidth) return text;
    std::size_t start = 0;
    while (start < text.size()) {
        do {
            ++start;
        } while (start < text.size() && (static_cast<unsigned char>(text[start]) & 0xC0) == 0x80);
        std::string candidate = "…" + text.substr(start);
        if (TextSize(font, size, candidate.c_str()).x <= maxWidth) return candidate;
    }
    return "…";
}

std::string EllipsizeRight(ImFont* font, float size, const std::string& text, float maxWidth) {
    if (TextSize(font, size, text.c_str()).x <= maxWidth) return text;
    std::size_t end = text.size();
    while (end > 0) {
        do {
            --end;
        } while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xC0) == 0x80);
        std::string candidate = text.substr(0, end) + "…";
        if (TextSize(font, size, candidate.c_str()).x <= maxWidth) return candidate;
    }
    return "…";
}

void HLine(ImDrawList* dl, float x0, float x1, float y, ImU32 color) {
    y = Px(y);
    dl->AddRectFilled(ImVec2(Px(x0), y), ImVec2(Px(x1), y + Thin()), color);
}

void VLine(ImDrawList* dl, float x, float y0, float y1, ImU32 color) {
    x = Px(x);
    dl->AddRectFilled(ImVec2(x, Px(y0)), ImVec2(x + Thin(), Px(y1)), color);
}

bool Button(const char* label, ImVec2 size, ButtonKind kind, bool enabled) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    p = ImVec2(Px(p.x), Px(p.y));
    size = ImVec2(Px(size.x), Px(size.y));
    ImGui::SetCursorScreenPos(p);
    if (!enabled) ImGui::BeginDisabled();
    bool pressed = ImGui::InvisibleButton(label, size);
    bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && enabled;
    bool held = ImGui::IsItemActive();
    if (!enabled) ImGui::EndDisabled();
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    ImU32 bg = 0, border = 0, text = col::Text;
    switch (kind) {
        case ButtonKind::Primary:
            bg = !enabled ? col::Line : held ? col::WhiteActive : hovered ? col::WhiteHover : col::White;
            text = enabled ? col::OnWhite : col::TextFaint;
            break;
        case ButtonKind::Secondary:
            bg = !enabled ? 0 : held ? col::Selected : hovered ? col::Hover : 0;
            border = !enabled ? col::Line : hovered ? col::ControlHover : col::Control;
            text = enabled ? col::Text : col::TextFaint;
            break;
        case ButtonKind::Ghost:
            bg = held ? col::Selected : hovered ? col::Hover : 0;
            text = !enabled ? col::TextFaint : hovered ? col::Text : col::TextDim;
            break;
    }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 q(p.x + size.x, p.y + size.y);
    const float r = S(3);
    if (bg) dl->AddRectFilled(p, q, bg, r);
    if (border) dl->AddRect(ImVec2(p.x + 0.5f, p.y + 0.5f), ImVec2(q.x - 0.5f, q.y - 0.5f), border, r, Thin());

    std::string shown(label, LabelEnd(label));
    ImFont* font = GetFonts().bold;
    ImVec2 ts = TextSize(font, kFontSmall, shown.c_str());
    DrawLabel(dl, font, kFontSmall, ImVec2(p.x + (size.x - ts.x) * 0.5f, p.y + (size.y - ts.y) * 0.5f), text, shown.c_str());
    return pressed && enabled;
}

bool IconButton(const char* id, const char* icon, ImVec2 size, ImU32 hoverBg, ImU32 iconColor, ImU32 hoverIconColor) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool pressed = ImGui::InvisibleButton(id, size);
    bool hovered = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (hovered && hoverBg) dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), hoverBg);
    ImVec2 ts = TextSize(GetFonts().regular, kFontBody, icon);
    DrawLabel(dl, GetFonts().regular, kFontBody, ImVec2(p.x + (size.x - ts.x) * 0.5f, p.y + (size.y - ts.y) * 0.5f),
              hovered ? hoverIconColor : iconColor, icon);
    return pressed;
}

bool LinkButton(const char* label, float fontSize, ImU32 color) {
    std::string shown(label, LabelEnd(label));
    ImFont* font = GetFonts().regular;
    ImVec2 ts = TextSize(font, fontSize, shown.c_str());
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool pressed = ImGui::InvisibleButton(label, ts);
    bool hovered = ImGui::IsItemHovered();
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 c = hovered ? col::White : color ? color : col::Text;
    DrawLabel(dl, font, fontSize, p, c, shown.c_str());
    HLine(dl, p.x, p.x + ts.x, p.y + ts.y - S(1), hovered ? col::White : col::Control);
    return pressed;
}

void Spinner(ImDrawList* dl, ImVec2 center, float radius, float thickness, ImU32 color) {
    float t = static_cast<float>(ImGui::GetTime());
    float start = t * 5.f;
    float sweep = 3.4f + std::sin(t * 2.f) * 1.1f;
    dl->PathClear();
    dl->PathArcTo(center, radius, start, start + sweep, 32);
    dl->PathStroke(color, thickness);
}

void ProgressBar(ImVec2 size, float fraction, bool indeterminate) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    p = ImVec2(Px(p.x), Px(p.y));
    size = ImVec2(Px(size.x), std::max(1.f, Px(size.y)));
    ImGui::Dummy(size);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 q(p.x + size.x, p.y + size.y);
    dl->AddRectFilled(p, q, col::Line);
    if (indeterminate) {
        float t = static_cast<float>(ImGui::GetTime());
        float w = size.x * 0.22f;
        float x = p.x + std::fmod(t * size.x * 0.55f, size.x + w) - w;
        float x0 = std::max(p.x, x), x1 = std::min(q.x, x + w);
        if (x1 > x0) dl->AddRectFilled(ImVec2(x0, p.y), ImVec2(x1, q.y), col::White);
        return;
    }
    float fx = Px(p.x + size.x * std::clamp(fraction, 0.f, 1.f));
    if (fx > p.x) dl->AddRectFilled(p, ImVec2(fx, q.y), col::White);
}

void DrawCheckbox(ImDrawList* dl, ImVec2 pos, bool checked, bool hovered, bool enabled) {
    const float s = Px(S(kMarkSize));
    ImVec2 p(Px(pos.x), Px(pos.y)), q(p.x + s, p.y + s);
    const float r = S(2.5f);
    if (checked) {
        dl->AddRectFilled(p, q, !enabled ? col::Control : hovered ? col::WhiteHover : col::White, r);
        // Check mark drawn as a polyline: crisper than a glyph at this size.
        ImVec2 pts[3] = {ImVec2(p.x + s * 0.24f, p.y + s * 0.52f), ImVec2(p.x + s * 0.43f, p.y + s * 0.70f),
                         ImVec2(p.x + s * 0.77f, p.y + s * 0.32f)};
        dl->AddPolyline(pts, 3, col::OnWhite, std::max(1.5f, S(1.9f)));
    } else {
        ImU32 border = !enabled ? col::Line : hovered ? col::ControlHover : col::Control;
        dl->AddRect(ImVec2(p.x + 0.5f, p.y + 0.5f), ImVec2(q.x - 0.5f, q.y - 0.5f), border, r, std::max(1.f, S(1.25f)));
    }
}

void DrawRadio(ImDrawList* dl, ImVec2 pos, bool selected, bool hovered) {
    const float s = Px(S(kMarkSize));
    ImVec2 c(Px(pos.x) + s * 0.5f, Px(pos.y) + s * 0.5f);
    ImU32 ring = selected ? col::White : hovered ? col::ControlHover : col::Control;
    dl->AddCircle(c, s * 0.5f - 0.5f, ring, 32, std::max(1.f, S(1.25f)));
    if (selected) dl->AddCircleFilled(c, s * 0.25f, col::White, 24);
}

}  // namespace uf::ui
