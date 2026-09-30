#pragma once
#include <imgui.h>

#include <string>

namespace uf::ui {

void SetUiScale(float scale);
float UiScale();
// Logical pixels -> physical pixels.
inline float S(float v) { return v * UiScale(); }

ImU32 WithAlpha(ImU32 c, float alpha);

// Text helpers working in logical font sizes (scaled internally).
ImVec2 TextSize(ImFont* font, float size, const char* text, float wrapWidth = 0.f);
void DrawLabel(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, ImU32 color, const char* text, float wrapWidth = 0.f);
// Shortens a path from the left: "…\Arizona Games Launcher\bin\arizona".
std::string EllipsizeLeft(ImFont* font, float size, const std::string& text, float maxWidth);
std::string EllipsizeRight(ImFont* font, float size, const std::string& text, float maxWidth);

// Pill-shaped tinted label; returns its width.
float Badge(ImDrawList* dl, ImVec2 pos, const char* text, ImU32 color);
float BadgeWidth(const char* text);

enum class ButtonKind { Primary, Secondary, Ghost, Danger };
bool Button(const char* label, ImVec2 size, ButtonKind kind = ButtonKind::Secondary, bool enabled = true);
bool IconButton(const char* id, const char* icon, ImVec2 size, ImU32 hoverBg, ImU32 iconColor, ImU32 hoverIconColor);

void Spinner(ImDrawList* dl, ImVec2 center, float radius, float thickness, ImU32 color);
void ProgressBar(ImVec2 size, float fraction, bool indeterminate = false);
// iOS-like switch. Returns true when toggled.
bool Toggle(const char* id, bool* value, bool enabled = true);
void IconCircle(ImDrawList* dl, ImVec2 center, float radius, ImU32 color, const char* icon, float iconSize);

// Wrapped text in the current window with a given color and font size.
void TextWrapped(const char* text, ImU32 color, float size, ImFont* font = nullptr);

}  // namespace uf::ui
