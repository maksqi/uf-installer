#include <algorithm>
#include <format>

#include "payload_manifest.gen.h"
#include "ui/IconsFontAwesome6.h"
#include "ui/screens/screens.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace uf::ui {

namespace {

void LogBox(const std::vector<std::string>& lines, ImVec2 size, bool followTail) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), col::Surface, S(10));
    ImGui::SetCursorScreenPos(ImVec2(p.x + S(12), p.y + S(10)));
    ImGui::BeginChild("##log", ImVec2(size.x - S(18), size.y - S(20)), ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
    ImGui::PushFont(GetFonts().regular, kFontTiny);
    ImGui::PushStyleColor(ImGuiCol_Text, col::TextDim);
    ImGui::PushTextWrapPos(0.f);
    for (const std::string& line : lines) ImGui::TextUnformatted(line.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
    ImGui::PopFont();
    if (followTail && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - S(40)) ImGui::SetScrollHereY(1.f);
    ImGui::EndChild();
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + size.y + S(10)));
}

void CenteredText(ImDrawList* dl, ImFont* font, float size, float cx, float y, ImU32 color, const char* text) {
    ImVec2 ts = TextSize(font, size, text);
    DrawLabel(dl, font, size, ImVec2(cx - ts.x * 0.5f, y), color, text);
}

}  // namespace

void DrawProgressScreen(App& app) {
    Layout l = GetLayout();
    Fonts& f = GetFonts();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float w = std::min(S(640), l.size.x - l.margin * 2);
    const float x = l.origin.x + (l.size.x - w) * 0.5f;
    const float cx = l.origin.x + l.size.x * 0.5f;
    float y = l.top + S(40);

    Spinner(dl, ImVec2(cx, y + S(26)), S(24), S(4), col::Accent);
    y += S(70);
    CenteredText(dl, f.bold, kFontHeading, cx, y, col::Text, "Установка…");
    y += S(38);
    std::string where = std::string("в ") + PathUtf8(app.target);
    where = EllipsizeLeft(f.regular, kFontSmall, where, w);
    CenteredText(dl, f.regular, kFontSmall, cx, y, col::TextDim, where.c_str());
    y += S(40);

    const InstallProgress& p = app.progress;
    std::string stage = p.stage.empty() ? "Подготовка" : p.stage;
    DrawLabel(dl, f.bold, kFontSmall, ImVec2(x, y), col::Text, stage.c_str());
    std::string pct = std::format("{:.0f}%", p.fraction * 100.f);
    ImVec2 ps = TextSize(f.bold, kFontSmall, pct.c_str());
    DrawLabel(dl, f.bold, kFontSmall, ImVec2(x + w - ps.x, y), col::Accent, pct.c_str());
    y += S(24);
    ImGui::SetCursorScreenPos(ImVec2(x, y));
    ProgressBar(ImVec2(w, S(10)), p.fraction, p.stage == "DirectX");
    y += S(20);
    std::string current = EllipsizeLeft(f.regular, kFontTiny, p.current, w);
    DrawLabel(dl, f.regular, kFontTiny, ImVec2(x, y), col::TextFaint, current.c_str());
    y += S(30);

    float footerH = S(76);
    float logH = l.origin.y + l.size.y - footerH - y - S(14);
    ImGui::SetCursorScreenPos(ImVec2(x, y));
    LogBox(app.installLog, ImVec2(w, logH), true);

    float fy = BeginFooter(l, footerH);
    DrawLabel(dl, f.regular, kFontSmall, ImVec2(l.origin.x + l.margin, fy + footerH * 0.5f - S(9)), col::TextDim,
             ICON_FA_CIRCLE_INFO "  Не закрывайте установщик и не запускайте игру до окончания.");
    ImGui::SetCursorScreenPos(ImVec2(l.origin.x + l.size.x - l.margin - S(120), fy + (footerH - S(40)) * 0.5f));
    if (Button("Отмена", ImVec2(S(120), S(40)), ButtonKind::Secondary, p.stage != "DirectX")) app.CancelInstall();
}

void DrawDoneScreen(App& app) {
    Layout l = GetLayout();
    Fonts& f = GetFonts();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (!app.result) return;
    const InstallResult& r = *app.result;
    const float w = std::min(S(660), l.size.x - l.margin * 2);
    const float x = l.origin.x + (l.size.x - w) * 0.5f;
    const float cx = l.origin.x + l.size.x * 0.5f;
    const float footerH = S(76);
    float y = l.top + S(36);

    if (r.ok) {
        // soft glow + check mark
        dl->AddCircleFilled(ImVec2(cx, y + S(34)), S(46), WithAlpha(col::Ok, 0.06f), 48);
        IconCircle(dl, ImVec2(cx, y + S(34)), S(34), col::Ok, ICON_FA_CHECK, 28.f);
        y += S(88);
        CenteredText(dl, f.bold, kFontHeading, cx, y, col::Text, "Готово!");
        y += S(38);
        std::string sub = std::format("UltraFuck {} и всё нужное для него установлены.", gen::kScriptVersion);
        CenteredText(dl, f.regular, kFontBody, cx, y, col::TextDim, sub.c_str());
        y += S(34);

        std::string stats = std::format("Новых файлов: {}  ·  заменено: {}  ·  без изменений: {}", r.installed, r.replaced, r.skipped);
        if (r.movedOld) stats += std::format("  ·  старых версий убрано: {}", r.movedOld);
        CenteredText(dl, f.regular, kFontSmall, cx, y, col::Text, stats.c_str());
        y += S(24);
        std::string extra;
        if (r.fontsInstalled) extra += std::format("Шрифтов установлено: {}", r.fontsInstalled);
        if (r.directxInstalled) extra += std::string(extra.empty() ? "" : "  ·  ") + "DirectX установлен";
        extra += std::string(extra.empty() ? "" : "  ·  ") + std::format("за {:.1f} с", r.seconds);
        CenteredText(dl, f.regular, kFontSmall, cx, y, col::TextFaint, extra.c_str());
        y += S(34);

        ImGui::SetCursorScreenPos(ImVec2(x, y));
        ImGui::BeginChild("##done", ImVec2(w + S(8), l.origin.y + l.size.y - footerH - y - S(10)), ImGuiChildFlags_None,
                          ImGuiWindowFlags_NoBackground);
        for (const std::string& warn : r.warnings) DrawBanner(Severity::Warning, warn.c_str(), nullptr, w);
        if (!r.backupDir.empty()) {
            std::string t = "Заменённые и старые файлы сохранены в резервную копию: " + PathUtf8(r.backupDir);
            if (DrawBanner(Severity::Info, t.c_str(), "Открыть", w)) app.OpenInExplorer(r.backupDir);
        }
        DrawBanner(Severity::Info, "Запускайте игру как обычно — скрипт загрузится вместе с MoonLoader.", nullptr, w);
        ImGui::EndChild();
    } else {
        IconCircle(dl, ImVec2(cx, y + S(34)), S(34), col::Err, ICON_FA_XMARK, 28.f);
        y += S(88);
        CenteredText(dl, f.bold, kFontHeading, cx, y, col::Text, "Установка не удалась");
        y += S(38);
        const char* sub = r.rolledBack ? "Все изменения отменены — папка игры в исходном состоянии." : "Папка игры не изменена.";
        CenteredText(dl, f.regular, kFontBody, cx, y, col::TextDim, sub);
        y += S(40);
        ImGui::SetCursorScreenPos(ImVec2(x, y));
        ImGui::BeginChild("##fail", ImVec2(w + S(8), l.origin.y + l.size.y - footerH - y - S(10)), ImGuiChildFlags_None,
                          ImGuiWindowFlags_NoBackground);
        DrawBanner(Severity::Error, r.error.c_str(), nullptr, w);
        LogBox(app.installLog, ImVec2(w, S(150)), false);
        ImGui::EndChild();
    }

    float fy = BeginFooter(l, footerH);
    float by = fy + (footerH - S(40)) * 0.5f;
    float rx = l.origin.x + l.size.x - l.margin;
    ImGui::SetCursorScreenPos(ImVec2(l.origin.x + l.margin, by));
    if (Button(ICON_FA_FILE_LINES "  Журнал", ImVec2(S(120), S(40)), ButtonKind::Ghost)) app.OpenLog();

    ImGui::SetCursorScreenPos(ImVec2(rx - S(130), by));
    if (Button("Закрыть", ImVec2(S(130), S(40)), ButtonKind::Primary)) app.RequestQuit();
    if (r.ok) {
        ImGui::SetCursorScreenPos(ImVec2(rx - S(130) - S(10) - S(200), by));
        if (Button(ICON_FA_FOLDER_OPEN "  Открыть папку игры", ImVec2(S(200), S(40)), ButtonKind::Secondary)) app.OpenInExplorer(app.target);
    } else {
        ImGui::SetCursorScreenPos(ImVec2(rx - S(130) - S(10) - S(170), by));
        if (Button(ICON_FA_ROTATE_LEFT "  Попробовать снова", ImVec2(S(170), S(40)), ButtonKind::Secondary)) app.OpenAnalysis(app.target);
    }
}

}  // namespace uf::ui
