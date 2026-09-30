#include <algorithm>
#include <format>
#include <utility>
#include <vector>

#include "payload_manifest.gen.h"
#include "ui/icons.h"
#include "ui/screens/screens.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace uf::ui {

namespace {

// Install log in a thin frame, mono font.
void LogBox(const std::vector<std::string>& lines, ImVec2 size, bool followTail) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    p = ImVec2(Px(p.x), Px(p.y));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRect(ImVec2(p.x + 0.5f, p.y + 0.5f), ImVec2(p.x + size.x - 0.5f, p.y + size.y - 0.5f), col::Line, 0.f,
                std::max(1.f, Px(S(1))));
    ImGui::SetCursorScreenPos(ImVec2(p.x + S(14), p.y + S(10)));
    ImGui::BeginChild("##log", ImVec2(size.x - S(20), size.y - S(20)), ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
    ImGui::PushFont(GetFonts().mono, kFontMono);
    ImGui::PushStyleColor(ImGuiCol_Text, col::TextDim);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, S(5)));
    ImGui::PushTextWrapPos(0.f);
    for (const std::string& line : lines) ImGui::TextUnformatted(line.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    ImGui::PopFont();
    if (followTail && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - S(40)) ImGui::SetScrollHereY(1.f);
    ImGui::EndChild();
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + size.y));
    ImGui::Dummy(ImVec2(size.x, S(10)));
}

// Heading with an optional icon in front: "✓ Готово".
void Heading(ImDrawList* dl, ImVec2 p, const char* icon, ImU32 iconColor, const char* text) {
    Fonts& f = GetFonts();
    float x = p.x;
    if (icon) {
        ImVec2 is = TextSize(f.regular, kFontHeading, icon);
        ImVec2 ts = TextSize(f.bold, kFontHeading, text);
        DrawLabel(dl, f.regular, kFontHeading, ImVec2(x, p.y + (ts.y - is.y) * 0.5f), iconColor, icon);
        x += is.x + S(10);
    }
    DrawLabel(dl, f.bold, kFontHeading, ImVec2(x, p.y), col::Text, text);
}

// Two-column key / value list with hairlines.
float StatsTable(ImDrawList* dl, ImVec2 p, float width, const std::vector<std::pair<std::string, std::string>>& rows) {
    Fonts& f = GetFonts();
    const float rowH = Px(S(32));
    HLine(dl, p.x, p.x + width, p.y, col::Line);
    for (std::size_t i = 0; i < rows.size(); ++i) {
        float y = p.y + i * rowH;
        float ty = y + (rowH - TextSize(f.regular, kFontSmall, "Ag").y) * 0.5f;
        DrawLabel(dl, f.regular, kFontSmall, ImVec2(p.x, ty), col::TextDim, rows[i].first.c_str());
        ImVec2 vs = TextSize(f.mono, kFontSmall, rows[i].second.c_str());
        DrawLabel(dl, f.mono, kFontSmall, ImVec2(p.x + width - vs.x, y + (rowH - vs.y) * 0.5f), col::Text, rows[i].second.c_str());
        HLine(dl, p.x, p.x + width, y + rowH - S(1), col::Line);
    }
    return rows.size() * rowH;
}

}  // namespace

void DrawProgressScreen(App& app) {
    Layout l = GetLayout();
    Fonts& f = GetFonts();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float x = l.origin.x + l.margin;
    const float w = l.size.x - l.margin * 2;
    const float footerH = S(72);
    float y = l.top + S(28);

    DrawLabel(dl, f.bold, kFontHeading, ImVec2(x, y), col::Text, "Установка");
    y += S(42);
    std::string where = EllipsizeLeft(f.mono, kFontMono, PathUtf8(app.target), w);
    DrawLabel(dl, f.mono, kFontMono, ImVec2(x, y), col::TextFaint, where.c_str());
    y += S(44);

    const InstallProgress& p = app.progress;
    std::string stage = p.stage.empty() ? "Подготовка" : p.stage;
    DrawLabel(dl, f.regular, kFontBody, ImVec2(x, y), col::Text, stage.c_str());
    bool indeterminate = p.stage == "DirectX";
    if (!indeterminate) {
        std::string pct = std::format("{:.0f}%", p.fraction * 100.f);
        ImVec2 ps = TextSize(f.mono, kFontBody, pct.c_str());
        DrawLabel(dl, f.mono, kFontBody, ImVec2(x + w - ps.x, y), col::Text, pct.c_str());
    }
    y += S(30);
    ImGui::SetCursorScreenPos(ImVec2(x, y));
    ProgressBar(ImVec2(w, S(3)), p.fraction, indeterminate);
    y += S(14);
    std::string current = EllipsizeLeft(f.mono, kFontMono, p.current, w);
    DrawLabel(dl, f.mono, kFontMono, ImVec2(x, y), col::TextFaint, current.c_str());
    y += S(34);

    float logH = l.origin.y + l.size.y - footerH - y - S(20);
    ImGui::SetCursorScreenPos(ImVec2(x, y));
    LogBox(app.installLog, ImVec2(w, logH), true);

    float fy = BeginFooter(l, footerH);
    float lh = TextSize(f.regular, kFontSmall, "Ag").y;
    DrawLabel(dl, f.regular, kFontSmall, ImVec2(x, fy + (footerH - lh) * 0.5f), col::TextDim,
              "Не закрывайте установщик и не запускайте игру до окончания.");
    ImGui::SetCursorScreenPos(ImVec2(l.origin.x + l.size.x - l.margin - S(112), fy + (footerH - S(36)) * 0.5f));
    if (Button("Отмена", ImVec2(S(112), S(36)), ButtonKind::Secondary, !indeterminate)) app.CancelInstall();
}

void DrawDoneScreen(App& app) {
    Layout l = GetLayout();
    Fonts& f = GetFonts();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (!app.result) return;
    const InstallResult& r = *app.result;
    const float x = l.origin.x + l.margin;
    const float w = l.size.x - l.margin * 2;
    const float footerH = S(72);
    float y = l.top + S(28);

    if (r.ok) {
        Heading(dl, ImVec2(x, y), ICON_CHECK_CIRCLE, col::Text, "Готово");
        y += S(42);
        std::string sub = std::format("UltraFuck {} и всё нужное для него установлено. Запускайте игру как обычно.", gen::kScriptVersion);
        DrawLabel(dl, f.regular, kFontBody, ImVec2(x, y), col::TextDim, sub.c_str());
        y += S(40);

        ImGui::SetCursorScreenPos(ImVec2(x, y));
        ImGui::BeginChild("##done", ImVec2(w + S(10), l.origin.y + l.size.y - footerH - y - S(6)), ImGuiChildFlags_None,
                          ImGuiWindowFlags_NoBackground);
        ImDrawList* cdl = ImGui::GetWindowDrawList();
        std::vector<std::pair<std::string, std::string>> stats = {
            {"Новых файлов", std::to_string(r.installed)},
            {"Заменено", std::to_string(r.replaced)},
            {"Без изменений", std::to_string(r.skipped)},
        };
        if (r.movedOld) stats.push_back({"Старых версий убрано", std::to_string(r.movedOld)});
        if (r.fontsInstalled) stats.push_back({"Шрифтов установлено", std::to_string(r.fontsInstalled)});
        if (r.directxInstalled) stats.push_back({"DirectX", "установлен"});
        stats.push_back({"Время", std::format("{:.1f} с", r.seconds)});
        ImVec2 p = ImGui::GetCursorScreenPos();
        float th = StatsTable(cdl, p, std::min(w, S(380)), stats);
        ImGui::Dummy(ImVec2(w, th + S(20)));

        for (const std::string& warn : r.warnings) DrawBanner(Severity::Warning, warn.c_str(), nullptr, w);
        if (!r.backupDir.empty()) {
            std::string t = "Заменённые и старые файлы сохранены в резервную копию: " + PathUtf8(r.backupDir);
            if (DrawBanner(Severity::Info, t.c_str(), "Открыть", w)) app.OpenInExplorer(r.backupDir);
        }
        ImGui::EndChild();
    } else {
        Heading(dl, ImVec2(x, y), ICON_WARNING_CIRCLE, col::Err, "Установка не удалась");
        y += S(42);
        const char* sub = r.rolledBack ? "Все изменения отменены, папка игры в исходном состоянии." : "Папка игры не изменена.";
        DrawLabel(dl, f.regular, kFontBody, ImVec2(x, y), col::TextDim, sub);
        y += S(40);
        ImGui::SetCursorScreenPos(ImVec2(x, y));
        ImGui::BeginChild("##fail", ImVec2(w + S(10), l.origin.y + l.size.y - footerH - y - S(6)), ImGuiChildFlags_None,
                          ImGuiWindowFlags_NoBackground);
        DrawBanner(Severity::Error, r.error.c_str(), nullptr, w);
        LogBox(app.installLog, ImVec2(w, S(170)), false);
        ImGui::EndChild();
    }

    float fy = BeginFooter(l, footerH);
    float by = fy + (footerH - S(36)) * 0.5f;
    float rx = l.origin.x + l.size.x - l.margin;
    ImGui::SetCursorScreenPos(ImVec2(x - S(12), by));
    if (Button(ICON_FILE_TEXT "  Журнал", ImVec2(S(112), S(36)), ButtonKind::Ghost)) app.OpenLog();

    ImGui::SetCursorScreenPos(ImVec2(rx - S(112), by));
    if (Button("Закрыть", ImVec2(S(112), S(36)), ButtonKind::Primary)) app.RequestQuit();
    ImGui::SetCursorScreenPos(ImVec2(rx - S(112) - S(8) - S(184), by));
    if (r.ok) {
        if (Button(ICON_FOLDER_OPEN "  Открыть папку игры", ImVec2(S(184), S(36)), ButtonKind::Secondary)) app.OpenInExplorer(app.target);
    } else {
        if (Button(ICON_ARROW_COUNTER_CLOCKWISE "  Попробовать снова", ImVec2(S(184), S(36)), ButtonKind::Secondary))
            app.OpenAnalysis(app.target);
    }
}

}  // namespace uf::ui
