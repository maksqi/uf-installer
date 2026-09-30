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
    dl->AddText(font, S(size), pos, color, text, nullptr, wrapWidth);
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

float BadgeWidth(const char* text) { return TextSize(GetFonts().bold, kFontTiny, text).x + S(16); }

float Badge(ImDrawList* dl, ImVec2 pos, const char* text, ImU32 color) {
    ImVec2 ts = TextSize(GetFonts().bold, kFontTiny, text);
    ImVec2 size(ts.x + S(16), ts.y + S(6));
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), WithAlpha(color, 0.16f), size.y * 0.5f);
    dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), WithAlpha(color, 0.35f), size.y * 0.5f, S(1));
    DrawLabel(dl, GetFonts().bold, kFontTiny, ImVec2(pos.x + S(8), pos.y + S(3)), color, text);
    return size.x;
}

bool Button(const char* label, ImVec2 size, ButtonKind kind, bool enabled) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    if (!enabled) ImGui::BeginDisabled();
    bool pressed = ImGui::InvisibleButton(label, size);
    bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
    bool held = ImGui::IsItemActive();
    if (!enabled) ImGui::EndDisabled();
    if (hovered && enabled) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    ImU32 bg = 0, border = 0, text = col::Text;
    switch (kind) {
        case ButtonKind::Primary:
            bg = held ? col::AccentActive : hovered ? col::AccentHover : col::Accent;
            text = IM_COL32_WHITE;
            break;
        case ButtonKind::Secondary:
            bg = held ? col::Border : hovered ? col::SurfaceActive : col::SurfaceHover;
            border = col::Border;
            break;
        case ButtonKind::Ghost:
            bg = held ? col::SurfaceActive : hovered ? col::SurfaceHover : 0;
            text = hovered ? col::Text : col::TextDim;
            break;
        case ButtonKind::Danger:
            bg = held ? WithAlpha(col::Err, 0.8f) : hovered ? col::Err : WithAlpha(col::Err, 0.85f);
            text = IM_COL32_WHITE;
            break;
    }
    if (!enabled) {
        bg = kind == ButtonKind::Primary ? WithAlpha(col::Accent, 0.30f) : WithAlpha(bg, 0.5f);
        text = WithAlpha(text, 0.45f);
    }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 q(p.x + size.x, p.y + size.y);
    if (bg) dl->AddRectFilled(p, q, bg, S(8));
    if (border) dl->AddRect(p, q, border, S(8), S(1));
    if (kind == ButtonKind::Primary && enabled)  // subtle top highlight
        dl->AddRectFilled(p, ImVec2(q.x, p.y + size.y * 0.5f), WithAlpha(IM_COL32_WHITE, hovered ? 0.07f : 0.05f), S(8),
                          ImDrawFlags_RoundCornersTop);

    const char* end = LabelEnd(label);
    std::string shown(label, end);
    ImFont* font = GetFonts().bold;
    ImVec2 ts = TextSize(font, kFontBody, shown.c_str());
    DrawLabel(dl, font, kFontBody, ImVec2(p.x + (size.x - ts.x) * 0.5f, p.y + (size.y - ts.y) * 0.5f), text, shown.c_str());
    return pressed && enabled;
}

bool IconButton(const char* id, const char* icon, ImVec2 size, ImU32 hoverBg, ImU32 iconColor, ImU32 hoverIconColor) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool pressed = ImGui::InvisibleButton(id, size);
    bool hovered = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (hovered) dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), hoverBg);
    ImVec2 ts = TextSize(GetFonts().regular, kFontBody, icon);
    DrawLabel(dl, GetFonts().regular, kFontBody, ImVec2(p.x + (size.x - ts.x) * 0.5f, p.y + (size.y - ts.y) * 0.5f),
             hovered ? hoverIconColor : iconColor, icon);
    return pressed;
}

void Spinner(ImDrawList* dl, ImVec2 center, float radius, float thickness, ImU32 color) {
    float t = static_cast<float>(ImGui::GetTime());
    float start = t * 5.5f;
    float sweep = 3.6f + std::sin(t * 2.2f) * 1.2f;
    dl->PathClear();
    dl->PathArcTo(center, radius, start, start + sweep, 28);
    dl->PathStroke(color, thickness);
}

void ProgressBar(ImVec2 size, float fraction, bool indeterminate) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::Dummy(size);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 q(p.x + size.x, p.y + size.y);
    float r = size.y * 0.5f;
    dl->AddRectFilled(p, q, col::SurfaceActive, r);
    float t = static_cast<float>(ImGui::GetTime());
    if (indeterminate) {
        float w = size.x * 0.25f;
        float x = p.x + std::fmod(t * size.x * 0.6f, size.x + w) - w;
        float x0 = std::max(p.x, x), x1 = std::min(q.x, x + w);
        if (x1 > x0) dl->AddRectFilled(ImVec2(x0, p.y), ImVec2(x1, q.y), col::Accent, r);
        return;
    }
    fraction = std::clamp(fraction, 0.f, 1.f);
    float fx = p.x + size.x * fraction;
    if (fx - p.x < size.y) fx = std::min(q.x, p.x + size.y * (fraction > 0 ? 1.f : 0.f));
    if (fx <= p.x) return;
    dl->AddRectFilled(p, ImVec2(fx, q.y), col::Accent, r);
    // moving shine over the filled part
    float band = S(80);
    float sx = p.x + std::fmod(t * S(260), (fx - p.x) + band) - band;
    dl->PushClipRect(p, ImVec2(fx, q.y), true);
    dl->AddRectFilledMultiColor(ImVec2(sx, p.y), ImVec2(sx + band * 0.5f, q.y), WithAlpha(IM_COL32_WHITE, 0.f),
                                WithAlpha(IM_COL32_WHITE, 0.18f), WithAlpha(IM_COL32_WHITE, 0.18f), WithAlpha(IM_COL32_WHITE, 0.f));
    dl->AddRectFilledMultiColor(ImVec2(sx + band * 0.5f, p.y), ImVec2(sx + band, q.y), WithAlpha(IM_COL32_WHITE, 0.18f),
                                WithAlpha(IM_COL32_WHITE, 0.f), WithAlpha(IM_COL32_WHITE, 0.f), WithAlpha(IM_COL32_WHITE, 0.18f));
    dl->PopClipRect();
}

bool Toggle(const char* id, bool* value, bool enabled) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 size(S(38), S(22));
    if (!enabled) ImGui::BeginDisabled();
    bool pressed = ImGui::InvisibleButton(id, size);
    bool hovered = ImGui::IsItemHovered();
    if (!enabled) ImGui::EndDisabled();
    if (pressed && enabled) *value = !*value;
    if (hovered && enabled) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    // Animate the knob via per-id storage.
    ImGuiStorage* st = ImGui::GetStateStorage();
    ImGuiID key = ImGui::GetID(id);
    float anim = st->GetFloat(key, *value ? 1.f : 0.f);
    float target = *value ? 1.f : 0.f;
    anim += (target - anim) * std::min(1.f, ImGui::GetIO().DeltaTime * 14.f);
    if (std::fabs(target - anim) < 0.01f) anim = target;
    st->SetFloat(key, anim);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 off = hovered ? col::Muted : col::Border;
    ImVec4 a = ImGui::ColorConvertU32ToFloat4(off), b = ImGui::ColorConvertU32ToFloat4(hovered ? col::AccentHover : col::Accent);
    ImU32 track = ImGui::ColorConvertFloat4ToU32(ImVec4(a.x + (b.x - a.x) * anim, a.y + (b.y - a.y) * anim, a.z + (b.z - a.z) * anim, 1.f));
    if (!enabled) track = WithAlpha(track, 0.4f);
    float r = size.y * 0.5f;
    dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), track, r);
    float kx = p.x + r + (size.x - 2 * r) * anim;
    dl->AddCircleFilled(ImVec2(kx, p.y + r), r - S(3), enabled ? IM_COL32_WHITE : WithAlpha(IM_COL32_WHITE, 0.6f), 24);
    return pressed && enabled;
}

void IconCircle(ImDrawList* dl, ImVec2 center, float radius, ImU32 color, const char* icon, float iconSize) {
    dl->AddCircleFilled(center, radius, WithAlpha(color, 0.14f), 32);
    ImVec2 ts = TextSize(GetFonts().regular, iconSize, icon);
    DrawLabel(dl, GetFonts().regular, iconSize, ImVec2(center.x - ts.x * 0.5f, center.y - ts.y * 0.5f), color, icon);
}

void TextWrapped(const char* text, ImU32 color, float size, ImFont* font) {
    ImGui::PushFont(font ? font : GetFonts().regular, size);
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::PushTextWrapPos(0.f);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

}  // namespace uf::ui
