#include <algorithm>
#include <cctype>
#include <format>
#include <optional>
#include <vector>

#include "core/elevation.h"
#include "ui/icons.h"
#include "ui/screens/screens.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace uf::ui {

namespace {

constexpr float kNameCol = 200.f;  // logical width of the component name column

// Versions ("2.4 → 2.36", "026.5-beta") read better in mono; sentences stay in the UI font.
ImFont* StatusFont(const std::string& status) {
    return !status.empty() && std::isdigit(static_cast<unsigned char>(status[0])) ? GetFonts().mono : GetFonts().regular;
}

void SectionTitle(const char* text) {
    ImGui::Dummy(ImVec2(0, S(14)));
    ImVec2 p = ImGui::GetCursorScreenPos();
    DrawLabel(ImGui::GetWindowDrawList(), GetFonts().bold, kFontSmall, p, col::Text, text);
    ImGui::Dummy(ImVec2(0, TextSize(GetFonts().bold, kFontSmall, text).y + S(8)));
}

// One component that needs attention or an action. Toggleable rows work as a checkbox over the whole row.
// Returns the new checkbox value if the user clicked it.
std::optional<bool> ItemRow(const PlanItem& it, float width) {
    Fonts& f = GetFonts();
    StateVisual v = VisualFor(it.state);
    const float nameX = S(44), right = width - S(12);
    const float statusX = nameX + S(kNameCol);
    const bool showDetail = !it.detail.empty() && it.state != ItemState::Ok;
    const float detailW = right - nameX - S(120);
    const float detailH = showDetail ? TextSize(f.regular, kFontTiny, it.detail.c_str(), detailW).y + S(6) : 0.f;
    const float h = Px(S(46) + detailH);

    ImVec2 p = ImGui::GetCursorScreenPos();
    p = ImVec2(Px(p.x), Px(p.y));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    std::optional<bool> toggled;
    bool hovered = false;
    if (it.toggleable) {
        ImGui::PushID(static_cast<int>(it.id));
        if (ImGui::InvisibleButton("##row", ImVec2(width, h))) toggled = !it.enabled;
        ImGui::PopID();
        hovered = ImGui::IsItemHovered();
        if (hovered) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            dl->AddRectFilled(p, ImVec2(p.x + width, p.y + h), col::Hover);
        }
    } else {
        ImGui::Dummy(ImVec2(width, h));
    }
    HLine(dl, p.x, p.x + width, p.y + h - S(1), col::Line);

    const float lineH = TextSize(f.regular, kFontBody, "Ag").y;
    const float ty = p.y + S(13);
    if (it.toggleable) {
        DrawCheckbox(dl, ImVec2(p.x + S(14), ty + (lineH - S(kMarkSize)) * 0.5f), it.enabled, hovered);
    } else {
        ImVec2 is = TextSize(f.regular, kFontBody, v.icon);
        DrawLabel(dl, f.regular, kFontBody, ImVec2(p.x + S(14) + (S(kMarkSize) - is.x) * 0.5f, ty), v.color, v.icon);
    }
    bool dim = it.toggleable && !it.enabled;
    DrawLabel(dl, f.regular, kFontBody, ImVec2(p.x + nameX, ty), dim ? col::TextDim : col::Text, it.title.c_str());

    // Right: what will happen (+ shield when it needs administrator rights).
    std::string action = it.toggleable && !it.enabled ? "пропустить" : v.label;
    if (it.admin && it.enabled) action = std::string(ICON_SHIELD_CHECK " ") + action;
    ImVec2 as = TextSize(f.regular, kFontSmall, action.c_str());
    float ay = ty + (lineH - as.y) * 0.5f;
    ImU32 actionColor = dim ? col::TextFaint : it.toggleable ? col::Text : v.color;
    DrawLabel(dl, f.regular, kFontSmall, ImVec2(p.x + right - as.x, ay), actionColor, action.c_str());

    ImFont* sf = StatusFont(it.status);
    std::string status = EllipsizeRight(sf, kFontSmall, it.status, right - as.x - S(24) - statusX);
    float sy = ty + (lineH - TextSize(sf, kFontSmall, "Ag").y) * 0.5f;
    DrawLabel(dl, sf, kFontSmall, ImVec2(p.x + statusX, sy), dim ? col::TextFaint : col::TextDim, status.c_str());
    if (showDetail)
        DrawLabel(dl, f.regular, kFontTiny, ImVec2(p.x + nameX, ty + lineH + S(6)), dim ? col::TextFaint : col::TextDim,
                  it.detail.c_str(), detailW);
    return toggled;
}

// Component that needs nothing: state icon, name and version on one line.
void CompactCell(const PlanItem& it, ImVec2 p, float width, float height) {
    Fonts& f = GetFonts();
    StateVisual v = VisualFor(it.state);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float cy = p.y + height * 0.5f;
    ImVec2 is = TextSize(f.regular, kFontSmall, v.icon);
    DrawLabel(dl, f.regular, kFontSmall, ImVec2(p.x + S(14) + (S(kMarkSize) - is.x) * 0.5f, cy - is.y * 0.5f), v.color, v.icon);
    float x = p.x + S(44);
    ImVec2 ts = TextSize(f.regular, kFontSmall, it.title.c_str());
    DrawLabel(dl, f.regular, kFontSmall, ImVec2(x, cy - ts.y * 0.5f), col::Text, it.title.c_str());
    x += ts.x + S(10);
    ImFont* sf = StatusFont(it.status);
    std::string status = EllipsizeRight(sf, kFontTiny, it.status, p.x + width - S(12) - x);
    ImVec2 ss = TextSize(sf, kFontTiny, status.c_str());
    DrawLabel(dl, sf, kFontTiny, ImVec2(x, cy - ss.y * 0.5f), col::TextFaint, status.c_str());
}

std::string FormatMb(std::uint64_t bytes) { return std::format("{:.1f} МБ", bytes / 1048576.0); }

}  // namespace

void DrawAnalysisScreen(App& app) {
    static const bool elevated = IsProcessElevated();
    Layout l = GetLayout();
    Fonts& f = GetFonts();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float footerH = S(72);
    const FolderReport* r = app.report.get();
    const float x0 = l.origin.x + l.margin;

    // ---- Header: folder name, what it is, path
    float y = l.top + S(28);
    std::string title = r && r->arizona ? r->arizonaTitle : PathUtf8(app.target.filename());
    DrawLabel(dl, f.bold, kFontHeading, ImVec2(x0, y), col::Text, title.c_str());
    y += S(42);
    std::string meta;
    if (r) {
        meta = r->hasSamp ? "SA-MP " + std::string(SampVersionName(r->samp)) : "SA-MP не найден";
        if (r->arizona) meta += " · Arizona Launcher";
        if (elevated && app.args().elevated) meta += " · запущено от администратора";
    }
    std::string path = EllipsizeLeft(f.mono, kFontMono, PathUtf8(app.target), l.size.x - l.margin * 2);
    if (!meta.empty()) {
        DrawLabel(dl, f.regular, kFontSmall, ImVec2(x0, y), col::TextDim, meta.c_str());
        y += S(22);
    }
    DrawLabel(dl, f.mono, kFontMono, ImVec2(x0, y), col::TextFaint, path.c_str());
    y += S(26);

    float contentW = l.size.x - l.margin * 2;
    float bodyH = l.origin.y + l.size.y - footerH - y - S(6);

    if (app.analyzing || !r) {
        float cy = y + bodyH * 0.38f;
        Spinner(dl, ImVec2(x0 + S(7), cy), S(6.5f), S(1.5f), col::Text);
        DrawLabel(dl, f.regular, kFontBody, ImVec2(x0 + S(24), cy - TextSize(f.regular, kFontBody, "Ag").y * 0.5f), col::TextDim,
                  "Проверяем, что уже установлено…");
        BeginFooter(l, footerH);
        return;
    }

    // ---- Scrollable body
    ImGui::SetCursorScreenPos(ImVec2(x0, y));
    ImGui::BeginChild("##body", ImVec2(contentW + S(10), bodyH), ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    const float rowW = contentW;

    if (!app.analysisBanner.text.empty()) {
        ImGui::Dummy(ImVec2(0, S(6)));
        bool offer = app.analysisBanner.offerWithoutAdmin;
        if (DrawBanner(app.analysisBanner.severity, app.analysisBanner.text.c_str(), offer ? "Установить без них" : nullptr, rowW) &&
            offer) {
            ImGui::PopStyleVar();
            ImGui::EndChild();
            app.InstallWithoutAdmin();
            return;
        }
    }

    std::optional<std::pair<ItemId, bool>> change;
    std::vector<const PlanItem*> changes, fine;
    for (const PlanItem& it : app.plan.items) {
        if (it.toggleable || it.state == ItemState::Error || it.state == ItemState::Warning)
            changes.push_back(&it);
        else
            fine.push_back(&it);
    }
    // Problems first, then the actions in plan order.
    std::stable_partition(changes.begin(), changes.end(), [](const PlanItem* it) { return !it->toggleable; });
    if (!changes.empty()) {
        SectionTitle("Что будет сделано");
        ImVec2 p = ImGui::GetCursorScreenPos();
        HLine(dl = ImGui::GetWindowDrawList(), p.x, p.x + rowW, p.y, col::Line);
        ImGui::Dummy(ImVec2(0, S(1)));
        for (const PlanItem* it : changes)
            if (auto t = ItemRow(*it, rowW)) change = std::pair{it->id, *t};
    }
    if (!app.plan.notices.empty()) {
        SectionTitle("Обратите внимание");
        for (const Notice& n : app.plan.notices) DrawBanner(n.severity, n.text.c_str(), nullptr, rowW);
    }

    if (!fine.empty()) {
        SectionTitle("Уже установлено");
        const float cellW = rowW * 0.5f, cellH = Px(S(34));
        ImVec2 start = ImGui::GetCursorScreenPos();
        ImDrawList* cdl = ImGui::GetWindowDrawList();
        std::size_t rows = (fine.size() + 1) / 2;
        HLine(cdl, start.x, start.x + rowW, start.y, col::Line);
        for (std::size_t i = 0; i < fine.size(); ++i)
            CompactCell(*fine[i], ImVec2(start.x + (i % 2) * cellW, start.y + (i / 2) * cellH), cellW, cellH);
        for (std::size_t row = 1; row <= rows; ++row) HLine(cdl, start.x, start.x + rowW, start.y + row * cellH - S(1), col::Line);
        ImGui::Dummy(ImVec2(rowW, rows * cellH));
    }

    SectionTitle("Дополнительно");
    {
        ImDrawList* cdl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        const float h = S(52);
        bool pressed = ImGui::InvisibleButton("##overwriteLibs", ImVec2(rowW, h));
        bool hovered = ImGui::IsItemHovered();
        if (hovered) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            cdl->AddRectFilled(p, ImVec2(p.x + rowW, p.y + h), col::Hover);
        }
        if (pressed) {
            app.options.overwriteLibs = !app.options.overwriteLibs;
            change = std::pair{ItemId::Count, app.options.overwriteLibs};
        }
        float lineH = TextSize(f.regular, kFontBody, "Ag").y;
        DrawCheckbox(cdl, ImVec2(p.x + S(14), p.y + S(8) + (lineH - S(kMarkSize)) * 0.5f), app.options.overwriteLibs, hovered);
        DrawLabel(cdl, f.regular, kFontBody, ImVec2(p.x + S(44), p.y + S(8)), col::Text, "Перезаписать все библиотеки MoonLoader");
        DrawLabel(cdl, f.regular, kFontTiny, ImVec2(p.x + S(44), p.y + S(8) + lineH + S(3)), col::TextDim,
                  "Если скрипт падает из-за старых библиотек. Изменённые файлы сохранятся в резервной копии.");
    }
    ImGui::Dummy(ImVec2(0, S(12)));
    ImGui::PopStyleVar();
    ImGui::EndChild();

    if (change) {
        if (change->first != ItemId::Count) app.options.Set(change->first, change->second);
        app.RebuildPlan();
    }

    // ---- Footer: summary + buttons
    dl = ImGui::GetWindowDrawList();
    const InstallPlan& plan = app.plan;
    float fy = BeginFooter(l, footerH);
    float cy = fy + footerH * 0.5f;
    float lh = TextSize(f.regular, kFontSmall, "Ag").y;
    float leftW = l.size.x - l.margin * 2 - S(290);
    if (plan.blocked) {
        std::string t = EllipsizeRight(f.regular, kFontSmall, std::string(ICON_WARNING_CIRCLE "  ") + plan.blockReason, leftW);
        DrawLabel(dl, f.regular, kFontSmall, ImVec2(x0, cy - lh * 0.5f), col::Err, t.c_str());
    } else if (plan.Empty()) {
        DrawLabel(dl, f.regular, kFontSmall, ImVec2(x0, cy - lh * 0.5f), col::TextDim, ICON_CHECK "  Всё уже установлено, делать ничего не нужно.");
    } else {
        std::size_t files = 0;
        for (const FileOp& op : plan.files) files += op.kind == OpKind::Copy;
        std::string line1 = std::format("Будет записано файлов: {} · {}", files, FormatMb(plan.BytesToWrite()));
        if (!plan.fonts.empty()) line1 += std::format(" · шрифтов: {}", plan.fonts.size());
        if (plan.directx) line1 += " · DirectX";
        bool twoLines = plan.needsAdmin && !elevated;
        DrawLabel(dl, f.regular, kFontSmall, ImVec2(x0, twoLines ? cy - lh - S(1) : cy - lh * 0.5f), col::Text, line1.c_str());
        if (twoLines) {
            std::string reasons;
            for (const std::string& s : plan.adminReasons) reasons += (reasons.empty() ? "" : ", ") + s;
            std::string line2 = EllipsizeRight(f.regular, kFontTiny, "Нужны права администратора: " + reasons, leftW);
            DrawLabel(dl, f.regular, kFontTiny, ImVec2(x0, cy + S(3)), col::TextDim, line2.c_str());
        }
    }

    float installW = S(152), backW = S(104);
    float rx = l.origin.x + l.size.x - l.margin;
    float by = fy + (footerH - S(36)) * 0.5f;
    ImGui::SetCursorScreenPos(ImVec2(rx - installW - S(8) - backW, by));
    if (Button("Назад##footer", ImVec2(backW, S(36)), ButtonKind::Secondary)) {
        app.BackToSelect();
        return;
    }
    ImGui::SetCursorScreenPos(ImVec2(rx - installW, by));
    bool admin = plan.needsAdmin && !elevated && !app.args().noElevate;
    const char* label = admin ? ICON_SHIELD_CHECK "  Установить" : "Установить";
    if (Button(label, ImVec2(installW, S(36)), ButtonKind::Primary, !plan.blocked && !plan.Empty())) app.BeginInstall();
}

}  // namespace uf::ui
