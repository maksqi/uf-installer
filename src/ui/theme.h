#pragma once
#include <imgui.h>

namespace uf::ui {

// Monochrome palette: near-black background, white as the only accent.
// Color is reserved for problems: amber for warnings, red for errors.
namespace col {
inline constexpr ImU32 Bg = IM_COL32(12, 12, 12, 255);
inline constexpr ImU32 Hover = IM_COL32(20, 20, 20, 255);       // row / button hover
inline constexpr ImU32 Selected = IM_COL32(26, 26, 26, 255);    // selected row, pressed button
inline constexpr ImU32 Line = IM_COL32(36, 36, 36, 255);        // hairlines, tracks
inline constexpr ImU32 Control = IM_COL32(64, 64, 64, 255);     // control outlines
inline constexpr ImU32 ControlHover = IM_COL32(110, 110, 110, 255);
inline constexpr ImU32 Text = IM_COL32(240, 240, 240, 255);
inline constexpr ImU32 TextDim = IM_COL32(172, 172, 172, 255);
inline constexpr ImU32 TextFaint = IM_COL32(128, 128, 128, 255);
inline constexpr ImU32 White = IM_COL32(255, 255, 255, 255);
inline constexpr ImU32 WhiteHover = IM_COL32(225, 225, 225, 255);
inline constexpr ImU32 WhiteActive = IM_COL32(200, 200, 200, 255);
inline constexpr ImU32 OnWhite = IM_COL32(12, 12, 12, 255);     // text on white
inline constexpr ImU32 Warn = IM_COL32(230, 170, 70, 255);
inline constexpr ImU32 Err = IM_COL32(236, 92, 92, 255);
inline constexpr ImU32 CloseHover = IM_COL32(196, 43, 28, 255);  // Windows 11 close button
}  // namespace col

struct Fonts {
    ImFont* regular = nullptr;  // IBM Plex Sans + Phosphor
    ImFont* bold = nullptr;     // IBM Plex Sans SemiBold + Phosphor Bold
    ImFont* mono = nullptr;     // IBM Plex Mono: paths, versions, log
};
Fonts& GetFonts();

// Font sizes (logical pixels, before DPI scaling).
inline constexpr float kFontBody = 16.f;
inline constexpr float kFontSmall = 14.f;
inline constexpr float kFontTiny = 13.5f;
inline constexpr float kFontMono = 13.5f;
inline constexpr float kFontHeading = 26.f;

// Loads the UI fonts from the exe resources (once).
void LoadFonts();
// The app logo (IDI_APP icon) as a `px` x `px` texture; nullptr if the icon can't be read.
ImTextureData* AppLogo(int px);
// Unregisters the logo textures; call after ImGui_ImplDX9_Shutdown().
void ReleaseAppLogos();
// Resets the style to the theme and scales every size for `scale` (DPI).
void ApplyStyle(float scale);

}  // namespace uf::ui
