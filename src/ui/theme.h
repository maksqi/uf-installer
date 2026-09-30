#pragma once
#include <windows.h>

#include <imgui.h>

#include <optional>
#include <string_view>

#include "core/i18n.h"

namespace uf::ui {

enum class Theme { Dark, Light };

// Monochrome palette: black on white or white on black, the "primary" ink is the only accent.
// Color is reserved for problems: amber for warnings, red for errors. Filled by SetTheme().
namespace col {
inline ImU32 Bg, Hover, Selected;          // background, row / button hover, selected row
inline ImU32 Line, Control, ControlHover;  // hairlines and tracks, control outlines
inline ImU32 Text, TextDim, TextFaint;
inline ImU32 Primary, PrimaryHover, PrimaryActive, OnPrimary;  // main button, checkbox, progress; text on it
inline ImU32 Warn, Err;
inline constexpr ImU32 CloseHover = IM_COL32(196, 43, 28, 255);  // Windows 11 close button
}  // namespace col

Theme CurrentTheme();
void SetTheme(Theme theme);
// Light when Windows apps use the light mode (Windows 10+); dark otherwise.
Theme SystemTheme();
const char* ThemeName(Theme theme);                    // "dark" / "light"
std::optional<Theme> ParseTheme(std::string_view s);   // "dark" / "light"
// Switches the palette, the ImGui style and the window frame (DWM dark mode).
void ApplyTheme(Theme theme, HWND hwnd);

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
// The flag of `lang` (Russia / Ukraine / United Kingdom), `width` pixels wide and 4:3; nullptr if it can't be drawn.
ImTextureData* FlagTexture(Lang lang, int width);
// Unregisters the logo and flag textures; call after ImGui_ImplDX9_Shutdown().
void ReleaseTextures();
// Resets the style to the theme and scales every size for `scale` (DPI).
void ApplyStyle(float scale);

}  // namespace uf::ui
