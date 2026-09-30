#pragma once
#include <imgui.h>

#include "core/plan.h"
#include "ui/app.h"

namespace uf::ui {

void DrawTitleBar(App& app);
void DrawSelectScreen(App& app);
void DrawAnalysisScreen(App& app);
void DrawProgressScreen(App& app);
void DrawDoneScreen(App& app);

// ---- shared helpers (screens/common.cpp)
struct StateVisual {
    ImU32 color;
    const char* icon;
    const char* label;
};
StateVisual VisualFor(ItemState state);
StateVisual VisualFor(Severity severity);

// Content area below the title bar, in screen coordinates.
struct Layout {
    ImVec2 origin;  // top-left of the window
    ImVec2 size;    // window size
    float top;      // y (absolute) where content starts
    float margin;
};
Layout GetLayout();

// Colored message box at the cursor, full available width. Returns true if its button was pressed.
bool DrawBanner(Severity severity, const char* text, const char* buttonLabel = nullptr, float width = 0.f);
// Bottom bar: draws the separator and positions the cursor; returns the y where buttons go.
float BeginFooter(const Layout& l, float height);

}  // namespace uf::ui
