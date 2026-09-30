#pragma once
#include <imgui.h>

namespace uf::ui {

namespace col {
inline constexpr ImU32 Bg = IM_COL32(13, 15, 20, 255);
inline constexpr ImU32 TitleBar = IM_COL32(17, 19, 25, 255);
inline constexpr ImU32 Surface = IM_COL32(22, 25, 33, 255);
inline constexpr ImU32 SurfaceHover = IM_COL32(28, 32, 42, 255);
inline constexpr ImU32 SurfaceActive = IM_COL32(35, 40, 52, 255);
inline constexpr ImU32 Border = IM_COL32(40, 45, 58, 255);
inline constexpr ImU32 Text = IM_COL32(232, 234, 240, 255);
inline constexpr ImU32 TextDim = IM_COL32(150, 157, 172, 255);
inline constexpr ImU32 TextFaint = IM_COL32(100, 107, 122, 255);
inline constexpr ImU32 Accent = IM_COL32(139, 92, 246, 255);
inline constexpr ImU32 AccentHover = IM_COL32(157, 120, 250, 255);
inline constexpr ImU32 AccentActive = IM_COL32(118, 72, 226, 255);
inline constexpr ImU32 Accent2 = IM_COL32(79, 70, 229, 255);
inline constexpr ImU32 Ok = IM_COL32(34, 197, 94, 255);
inline constexpr ImU32 Warn = IM_COL32(245, 158, 11, 255);
inline constexpr ImU32 Err = IM_COL32(239, 68, 68, 255);
inline constexpr ImU32 Info = IM_COL32(59, 130, 246, 255);
inline constexpr ImU32 Muted = IM_COL32(100, 116, 139, 255);
}  // namespace col

struct Fonts {
    ImFont* regular = nullptr;
    ImFont* bold = nullptr;
};
Fonts& GetFonts();

// Font sizes (logical pixels, before DPI scaling).
inline constexpr float kFontBody = 15.f;
inline constexpr float kFontSmall = 13.f;
inline constexpr float kFontTiny = 12.f;
inline constexpr float kFontTitle = 15.5f;
inline constexpr float kFontHeading = 24.f;

// Loads Inter + Font Awesome from the exe resources (once).
void LoadFonts();
// Resets the style to the theme and scales every size for `scale` (DPI).
void ApplyStyle(float scale);

}  // namespace uf::ui
