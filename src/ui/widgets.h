#pragma once
#include <imgui.h>

#include <string>

namespace uf::ui {

void SetUiScale(float scale);
float UiScale();
// Logical pixels -> physical pixels.
inline float S(float v) { return v * UiScale(); }
// Snaps a coordinate to the pixel grid (crisp 1 px lines and text).
inline float Px(float v) { return static_cast<float>(static_cast<int>(v + 0.5f)); }

ImU32 WithAlpha(ImU32 c, float alpha);

// Text helpers working in logical font sizes (scaled internally).
ImVec2 TextSize(ImFont* font, float size, const char* text, float wrapWidth = 0.f);
void DrawLabel(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, ImU32 color, const char* text, float wrapWidth = 0.f);
// Shortens a path from the left: "…\Arizona Games Launcher\bin\arizona".
std::string EllipsizeLeft(ImFont* font, float size, const std::string& text, float maxWidth);
std::string EllipsizeRight(ImFont* font, float size, const std::string& text, float maxWidth);

// 1 px horizontal / vertical hairline.
void HLine(ImDrawList* dl, float x0, float x1, float y, ImU32 color);
void VLine(ImDrawList* dl, float x, float y0, float y1, ImU32 color);

enum class ButtonKind { Primary, Secondary, Ghost };
bool Button(const char* label, ImVec2 size, ButtonKind kind = ButtonKind::Secondary, bool enabled = true);
bool IconButton(const char* id, const char* icon, ImVec2 size, ImU32 hoverBg, ImU32 iconColor, ImU32 hoverIconColor);
// Inline text button (underlined on hover) at the cursor; returns true when clicked.
bool LinkButton(const char* label, float fontSize, ImU32 color = 0);

void Spinner(ImDrawList* dl, ImVec2 center, float radius, float thickness, ImU32 color);
void ProgressBar(ImVec2 size, float fraction, bool indeterminate = false);
// Square checkbox / round radio mark; the caller owns the clickable area (usually a whole row).
void DrawCheckbox(ImDrawList* dl, ImVec2 pos, bool checked, bool hovered, bool enabled = true);
void DrawRadio(ImDrawList* dl, ImVec2 pos, bool selected, bool hovered);
inline constexpr float kMarkSize = 16.f;  // logical size of a checkbox / radio

}  // namespace uf::ui
