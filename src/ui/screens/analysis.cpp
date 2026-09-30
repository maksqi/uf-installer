#include <format>
#include <optional>
#include <vector>

#include "core/elevation.h"
#include "ui/IconsFontAwesome6.h"
#include "ui/screens/screens.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace uf::ui {

namespace {

const char* ItemIcon(ItemId id) {
    switch (id) {
        case ItemId::Gta: return ICON_FA_CAR;
        case ItemId::Samp: return ICON_FA_PLUG;
        case ItemId::AsiLoader: return ICON_FA_PUZZLE_PIECE;
        case ItemId::Cleo: return ICON_FA_CODE;
        case ItemId::Sampfuncs: return ICON_FA_MICROCHIP;
        case ItemId::MoonLoader: return ICON_FA_TERMINAL;
        case ItemId::Libs: return ICON_FA_LAYER_GROUP;
        case ItemId::Script: return ICON_FA_BOLT;
        case ItemId::Fonts: return ICON_FA_FONT;
        case ItemId::DirectX: return ICON_FA_DISPLAY;
        default: return ICON_FA_CUBES;
    }
}

// Draws one component row; returns a new checkbox value if the user toggled it.
std::optional<bool> ItemRow(const PlanItem& it, float width) {
    Fonts& f = GetFonts();
    StateVisual v = VisualFor(it.state);
    const bool showDetail = !it.detail.empty() && it.state != ItemState::Ok;
    const float rightW = S(it.toggleable ? 190.f : 140.f);
    const float textX = S(56);
    const float detailW = width - textX - rightW;
    const float detailH = showDetail ? TextSize(f.regular, kFontTiny, it.detail.c_str(), detailW).y + S(4) : 0.f;
    const float h = S(52) + detailH;

    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + width, p.y + h), col::Surface, S(10));
    if (it.state == ItemState::Error)
        dl->AddRect(p, ImVec2(p.x + width, p.y + h), WithAlpha(col::Err, 0.35f), S(10), S(1));

    IconCircle(dl, ImVec2(p.x + S(28), p.y + S(26)), S(16), v.color, ItemIcon(it.id), 13.f);
    float x = p.x + textX;
    DrawLabel(dl, f.bold, kFontTitle, ImVec2(x, p.y + S(8)), col::Text, it.title.c_str());
    std::string status = EllipsizeRight(f.regular, kFontSmall, it.status, width - textX - rightW);
    DrawLabel(dl, f.regular, kFontSmall, ImVec2(x, p.y + S(28)), col::TextDim, status.c_str());
    if (showDetail) DrawLabel(dl, f.regular, kFontTiny, ImVec2(x, p.y + S(49)), col::TextFaint, it.detail.c_str(), detailW);

    // Right side: state chip (+ shield when admin rights are needed) and the switch.
    float rx = p.x + width - S(16);
    std::optional<bool> toggled;
    if (it.toggleable) {
        ImGui::SetCursorScreenPos(ImVec2(rx - S(38), p.y + S(15)));
        bool value = it.enabled;
        ImGui::PushID(static_cast<int>(it.id));
        if (Toggle("##on", &value)) toggled = value;
        ImGui::PopID();
        rx -= S(38) + S(12);
    }
    std::string label = v.label;
    if (it.admin && it.enabled) label = std::string(ICON_FA_SHIELD_HALVED " ") + label;
    float bw = BadgeWidth(label.c_str());
    Badge(dl, ImVec2(rx - bw, p.y + S(15)), label.c_str(), v.color);

    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h + S(6)));
    ImGui::Dummy(ImVec2(width, 0));
    return toggled;
}

// Small cell for components that need nothing: icon, title and version on one line.
void CompactCell(const PlanItem& it, ImVec2 p, float width) {
    Fonts& f = GetFonts();
    StateVisual v = VisualFor(it.state);
    const float h = S(42);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + width, p.y + h), col::Surface, S(10));
    IconCircle(dl, ImVec2(p.x + S(22), p.y + h * 0.5f), S(12), v.color, ItemIcon(it.id), 10.5f);
    float x = p.x + S(42);
    ImVec2 ts = TextSize(f.bold, kFontSmall, it.title.c_str());
    DrawLabel(dl, f.bold, kFontSmall, ImVec2(x, p.y + (h - ts.y) * 0.5f), col::Text, it.title.c_str());
    x += ts.x + S(8);
    std::string status = EllipsizeRight(f.regular, kFontSmall, it.status, p.x + width - S(14) - x);
    ImVec2 ss = TextSize(f.regular, kFontSmall, status.c_str());
    DrawLabel(dl, f.regular, kFontSmall, ImVec2(x, p.y + (h - ss.y) * 0.5f), col::TextDim, status.c_str());
}

void SectionLabel(const char* text) {
    ImGui::Dummy(ImVec2(0, S(4)));
    ImGui::PushFont(GetFonts().bold, kFontTiny);
    ImGui::PushStyleColor(ImGuiCol_Text, col::TextFaint);
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0, S(2)));
}

std::string FormatMb(std::uint64_t bytes) { return std::format("{:.1f} МБ", bytes / 1048576.0); }

}  // namespace

void DrawAnalysisScreen(App& app) {
    static const bool elevated = IsProcessElevated();
    Layout l = GetLayout();
    Fonts& f = GetFonts();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float footerH = S(76);
    const FolderReport* r = app.report.get();

    // ---- Header: back button, title, badges, path
    float y = l.top + S(18);
    ImGui::SetCursorScreenPos(ImVec2(l.origin.x + l.margin - S(8), y + S(2)));
    bool canGoBack = !app.analyzing;
    if (IconButton("##back", ICON_FA_ARROW_LEFT, ImVec2(S(36), S(36)), col::SurfaceHover, col::TextDim, col::Text) && canGoBack) {
        app.BackToSelect();
        return;
    }
    float x = l.origin.x + l.margin + S(38);
    std::string title = r && r->arizona ? r->arizonaTitle : PathUtf8(app.target.filename());
    DrawLabel(dl, f.bold, 22.f, ImVec2(x, y), col::Text, title.c_str());
    float bx = x + TextSize(f.bold, 22.f, title.c_str()).x + S(12);
    if (r) {
        if (r->arizona) bx += Badge(dl, ImVec2(bx, y + S(5)), "Arizona Launcher", col::Accent) + S(6);
        if (r->hasSamp) bx += Badge(dl, ImVec2(bx, y + S(5)), ("SA-MP " + std::string(SampShortName(r->samp))).c_str(), col::Info) + S(6);
        if (elevated && app.args().elevated) Badge(dl, ImVec2(bx, y + S(5)), ICON_FA_SHIELD_HALVED " администратор", col::Warn);
    }
    std::string path = EllipsizeLeft(f.regular, kFontSmall, PathUtf8(app.target), l.size.x - l.margin * 2 - S(40));
    DrawLabel(dl, f.regular, kFontSmall, ImVec2(x, y + S(32)), col::TextDim, path.c_str());
    y += S(62);

    float contentW = l.size.x - l.margin * 2;
    float bodyH = l.origin.y + l.size.y - footerH - y - S(10);

    if (app.analyzing || !r) {
        ImVec2 c(l.origin.x + l.size.x * 0.5f, y + bodyH * 0.4f);
        Spinner(dl, c, S(22), S(3.5f), col::Accent);
        const char* t = "Проверяем, что уже установлено…";
        ImVec2 ts = TextSize(f.regular, kFontBody, t);
        DrawLabel(dl, f.regular, kFontBody, ImVec2(c.x - ts.x * 0.5f, c.y + S(40)), col::TextDim, t);
        BeginFooter(l, footerH);
        return;
    }

    // ---- Scrollable body
    ImGui::SetCursorScreenPos(ImVec2(l.origin.x + l.margin, y));
    ImGui::BeginChild("##body", ImVec2(contentW + S(10), bodyH), ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
    const float rowW = contentW - S(4);

    if (!app.analysisBanner.text.empty()) {
        bool offer = app.analysisBanner.offerWithoutAdmin;
        if (DrawBanner(app.analysisBanner.severity, app.analysisBanner.text.c_str(), offer ? "Установить без них" : nullptr, rowW) && offer) {
            ImGui::EndChild();
            app.InstallWithoutAdmin();
            return;
        }
    }

    std::optional<std::pair<ItemId, bool>> change;
    std::vector<const PlanItem*> problems, actions, fine;
    for (const PlanItem& it : app.plan.items) {
        if (it.toggleable)
            actions.push_back(&it);
        else if (it.state == ItemState::Error || it.state == ItemState::Warning)
            problems.push_back(&it);
        else
            fine.push_back(&it);
    }
    if (!problems.empty()) {
        SectionLabel("НУЖНО ВНИМАНИЕ");
        for (const PlanItem* it : problems) ItemRow(*it, rowW);
    }
    if (!actions.empty()) {
        SectionLabel("БУДЕТ СДЕЛАНО");
        for (const PlanItem* it : actions)
            if (auto t = ItemRow(*it, rowW)) change = std::pair{it->id, *t};
    }
    if (!app.plan.notices.empty()) {
        SectionLabel("ВАЖНО");
        for (const Notice& n : app.plan.notices) DrawBanner(n.severity, n.text.c_str(), nullptr, rowW);
    }

    if (!fine.empty()) {
        SectionLabel("УЖЕ В ПОРЯДКЕ");
        const float gap = S(8), cellW = (rowW - gap) * 0.5f, cellH = S(42);
        ImVec2 start = ImGui::GetCursorScreenPos();
        for (std::size_t i = 0; i < fine.size(); ++i)
            CompactCell(*fine[i], ImVec2(start.x + (i % 2) * (cellW + gap), start.y + (i / 2) * (cellH + gap)), cellW);
        std::size_t rows = (fine.size() + 1) / 2;
        ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + rows * (cellH + gap)));
        ImGui::Dummy(ImVec2(rowW, 0));
    }

    SectionLabel("ДОПОЛНИТЕЛЬНО");
    {
        ImDrawList* cdl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        bool v = app.options.overwriteLibs;
        if (Toggle("##overwriteLibs", &v)) {
            app.options.overwriteLibs = v;
            change = std::pair{ItemId::Count, v};
        }
        DrawLabel(cdl, f.regular, kFontSmall, ImVec2(p.x + S(50), p.y + S(2)), col::Text, "Перезаписать все библиотеки MoonLoader");
        DrawLabel(cdl, f.regular, kFontTiny, ImVec2(p.x + S(50), p.y + S(20)), col::TextFaint,
                 "Если скрипт падает из-за старых библиотек. Изменённые файлы сохранятся в резервной копии.");
        ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + S(46)));
        ImGui::Dummy(ImVec2(rowW, 0));
    }
    ImGui::EndChild();

    if (change) {
        if (change->first != ItemId::Count) app.options.Set(change->first, change->second);
        app.RebuildPlan();
    }

    // ---- Footer: summary + buttons
    const InstallPlan& plan = app.plan;
    float fy = BeginFooter(l, footerH);
    float sx = l.origin.x + l.margin;
    float cy = fy + footerH * 0.5f;
    if (plan.blocked) {
        std::string t = std::string(ICON_FA_CIRCLE_XMARK "  ") + plan.blockReason;
        DrawLabel(dl, f.bold, kFontSmall, ImVec2(sx, cy - S(9)), col::Err, t.c_str());
    } else if (plan.Empty()) {
        DrawLabel(dl, f.bold, kFontSmall, ImVec2(sx, cy - S(9)), col::Ok, ICON_FA_CIRCLE_CHECK "  Всё уже установлено — делать ничего не нужно.");
    } else {
        std::size_t files = 0;
        for (const FileOp& op : plan.files) files += op.kind == OpKind::Copy;
        std::string line1 = std::format("Будет записано файлов: {} ({})", files, FormatMb(plan.BytesToWrite()));
        if (!plan.fonts.empty()) line1 += std::format(" · шрифтов: {}", plan.fonts.size());
        if (plan.directx) line1 += " · DirectX";
        bool twoLines = plan.needsAdmin && !elevated;
        DrawLabel(dl, f.regular, kFontSmall, ImVec2(sx, cy - S(twoLines ? 18.f : 9.f)), col::Text, line1.c_str());
        if (twoLines) {
            std::string reasons;
            for (const std::string& s : plan.adminReasons) reasons += (reasons.empty() ? "" : ", ") + s;
            std::string line2 = std::string(ICON_FA_SHIELD_HALVED "  Потребуются права администратора: ") + reasons;
            line2 = EllipsizeRight(f.regular, kFontTiny, line2, l.size.x - l.margin * 2 - S(310));
            DrawLabel(dl, f.regular, kFontTiny, ImVec2(sx, cy + S(3)), col::Warn, line2.c_str());
        }
    }

    float installW = S(170), backW = S(110);
    float rx = l.origin.x + l.size.x - l.margin;
    float by = fy + (footerH - S(40)) * 0.5f;
    ImGui::SetCursorScreenPos(ImVec2(rx - installW - S(10) - backW, by));
    if (Button("Назад##footer", ImVec2(backW, S(40)), ButtonKind::Secondary)) {
        app.BackToSelect();
        return;
    }
    ImGui::SetCursorScreenPos(ImVec2(rx - installW, by));
    bool admin = plan.needsAdmin && !elevated && !app.args().noElevate;
    const char* label = admin ? ICON_FA_SHIELD_HALVED "  Установить" : ICON_FA_DOWNLOAD "  Установить";
    if (Button(label, ImVec2(installW, S(40)), ButtonKind::Primary, !plan.blocked && !plan.Empty())) app.BeginInstall();
}

}  // namespace uf::ui
