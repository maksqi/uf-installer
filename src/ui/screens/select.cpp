#include <format>

#include "core/versions.h"
#include "payload_manifest.gen.h"
#include "ui/icons.h"
#include "ui/screens/screens.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace uf::ui {

namespace {

// Table columns, widths in logical px; the game column takes the rest.
constexpr float kColMark = 44.f, kColSamp = 76.f, kColScript = 76.f, kColParts = 236.f, kPadRight = 12.f;
constexpr float kRowH = 60.f;

// Installed UltraFuck version for the "Скрипт" column ("" if none).
std::string ScriptVersion(const FolderReport& r) {
    if (r.ufExactInstalled) return gen::kScriptVersion;
    for (const UfScript& s : r.ufScripts)
        if (s.loadable) return s.version.empty() ? "?" : s.version;
    return {};
}

// What is missing, fitted into `maxWidth`: "нет CLEO, MoonLoader" -> "нет ASI, CLEO, SF, ML" -> "нет 5 из 6".
// Empty if everything is in place.
std::string MissingParts(const FolderReport& r, float maxWidth) {
    bool fonts = true;
    for (const FontState& f : r.fonts)
        if (f.bundled && f.required && !f.present) fonts = false;
    struct Part {
        const char* name;
        const char* shortName;
        bool ok;
    };
    const Part parts[] = {{"ASI Loader", "ASI", r.loader != LoaderKind::None && r.loader != LoaderKind::Missing},
                          {"CLEO", "CLEO", r.cleo.has_value()},
                          {"SAMPFUNCS", "SF", r.sampfuncs.has_value()},
                          {"MoonLoader", "ML", r.moonloader.has_value()},
                          {"шрифты", "шрифты", fonts},
                          {"DirectX", "DX", r.d3dx9}};
    std::string full, brief;
    int count = 0;
    for (const Part& part : parts) {
        if (part.ok) continue;
        full += (full.empty() ? "нет " : ", ") + std::string(part.name);
        brief += (brief.empty() ? "нет " : ", ") + std::string(part.shortName);
        ++count;
    }
    ImFont* font = GetFonts().regular;
    if (TextSize(font, kFontSmall, full.c_str()).x <= maxWidth) return full;
    if (TextSize(font, kFontSmall, brief.c_str()).x <= maxWidth) return brief;
    return std::format("нет {} из {}", count, std::size(parts));
}

void ColumnHeader(ImDrawList* dl, ImVec2 p, float width) {
    Fonts& f = GetFonts();
    float y = p.y;
    float right = p.x + width - S(kPadRight);
    float xParts = right - S(kColParts), xScript = xParts - S(kColScript), xSamp = xScript - S(kColSamp);
    DrawLabel(dl, f.regular, kFontTiny, ImVec2(p.x + S(kColMark), y), col::TextFaint, "Игра");
    DrawLabel(dl, f.regular, kFontTiny, ImVec2(xSamp, y), col::TextFaint, "SA-MP");
    DrawLabel(dl, f.regular, kFontTiny, ImVec2(xScript, y), col::TextFaint, "Скрипт");
    DrawLabel(dl, f.regular, kFontTiny, ImVec2(xParts, y), col::TextFaint, "Компоненты");
}

// Returns 1 on click, 2 on double click.
int GameRow(const GameEntry& e, bool selected, float width) {
    Fonts& f = GetFonts();
    ImVec2 p = ImGui::GetCursorScreenPos();
    p = ImVec2(Px(p.x), Px(p.y));
    float h = Px(S(kRowH));
    ImGui::PushID(PathUtf8(e.dir).c_str());
    bool clicked = ImGui::InvisibleButton("##row", ImVec2(width, h));
    bool hovered = ImGui::IsItemHovered();
    bool dbl = hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
    ImGui::PopID();
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 q(p.x + width, p.y + h);
    if (selected || hovered) dl->AddRectFilled(p, q, selected ? col::Selected : col::Hover);
    HLine(dl, p.x, q.x, q.y - S(1), col::Line);

    const float mark = S(kMarkSize);
    DrawRadio(dl, ImVec2(p.x + S(14), p.y + (h - mark) * 0.5f), selected, hovered);

    float right = q.x - S(kPadRight);
    float xParts = right - S(kColParts), xScript = xParts - S(kColScript), xSamp = xScript - S(kColSamp);
    float x = p.x + S(kColMark);
    float nameW = xSamp - x - S(16);

    // Name (+ launcher note) and path.
    std::string title = EllipsizeRight(f.bold, kFontBody, e.title, nameW);
    DrawLabel(dl, f.bold, kFontBody, ImVec2(x, p.y + S(11)), col::Text, title.c_str());
    if (!e.arizonaId.empty()) {
        float tx = x + TextSize(f.bold, kFontBody, title.c_str()).x + S(8);
        if (tx + S(110) < xSamp) DrawLabel(dl, f.regular, kFontTiny, ImVec2(tx, p.y + S(13)), col::TextFaint, "Arizona Launcher");
    }
    std::string path = EllipsizeLeft(f.mono, kFontMono, PathUtf8(e.dir), nameW);
    DrawLabel(dl, f.mono, kFontMono, ImVec2(x, p.y + S(34)), col::TextFaint, path.c_str());

    float cy = p.y + (h - TextSize(f.regular, kFontSmall, "Ag").y) * 0.5f;
    const FolderReport* r = e.report.get();
    if (!r) {
        Spinner(dl, ImVec2(xParts + S(6), p.y + h * 0.5f), S(5.5f), S(1.5f), col::TextDim);
        DrawLabel(dl, f.regular, kFontSmall, ImVec2(xParts + S(20), cy), col::TextFaint, "проверяем…");
        return dbl ? 2 : clicked ? 1 : 0;
    }
    float my = p.y + (h - TextSize(f.mono, kFontSmall, "0").y) * 0.5f;
    if (r->hasSamp)
        DrawLabel(dl, f.mono, kFontSmall, ImVec2(xSamp, my), col::Text, std::string(SampShortName(r->samp)).c_str());
    else
        DrawLabel(dl, f.regular, kFontSmall, ImVec2(xSamp, cy), col::Warn, "нет");

    std::string ver = ScriptVersion(*r);
    bool current = !ver.empty() && (r->ufExactInstalled || CompareDecimalVersions(ver, gen::kScriptVersion) == 0);
    DrawLabel(dl, f.mono, kFontSmall, ImVec2(xScript, my), ver.empty() ? col::TextFaint : current ? col::TextDim : col::Text,
              ver.empty() ? "—" : ver.c_str());

    std::string missing = MissingParts(*r, right - xParts);
    if (missing.empty())
        DrawLabel(dl, f.regular, kFontSmall, ImVec2(xParts, cy), col::TextDim, "все на месте");
    else
        DrawLabel(dl, f.regular, kFontSmall, ImVec2(xParts, cy), col::Text, missing.c_str());
    return dbl ? 2 : clicked ? 1 : 0;
}

void EmptyState(bool searching, ImVec2 p, float width, float height) {
    Fonts& f = GetFonts();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float cx = p.x + width * 0.5f, y = p.y + height * 0.32f;
    const char* title = searching ? "Ищем GTA San Andreas…" : "GTA San Andreas не найдена";
    const char* text = searching ? "Проверяем лаунчеры, реестр и диски компьютера."
                                 : "Нажмите «Обзор…» и укажите папку с игрой — ту, где лежит gta_sa.exe.";
    if (searching)
        Spinner(dl, ImVec2(cx, y), S(12), S(1.75f), col::Text);
    else {
        ImVec2 is = TextSize(f.regular, 28.f, ICON_MAGNIFYING_GLASS);
        DrawLabel(dl, f.regular, 28.f, ImVec2(cx - is.x * 0.5f, y - is.y * 0.5f), col::TextFaint, ICON_MAGNIFYING_GLASS);
    }
    ImVec2 ts = TextSize(f.bold, kFontBody, title);
    DrawLabel(dl, f.bold, kFontBody, ImVec2(cx - ts.x * 0.5f, y + S(30)), col::Text, title);
    ImVec2 ds = TextSize(f.regular, kFontSmall, text);
    DrawLabel(dl, f.regular, kFontSmall, ImVec2(cx - ds.x * 0.5f, y + S(54)), col::TextDim, text);
}

}  // namespace

void DrawSelectScreen(App& app) {
    Layout l = GetLayout();
    Fonts& f = GetFonts();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float footerH = S(72);
    Discovery* d = app.discovery();

    float x0 = l.origin.x + l.margin;
    float y = l.top + S(28);
    DrawLabel(dl, f.bold, kFontHeading, ImVec2(x0, y), col::Text, "Куда установить?");
    y += S(42);
    DrawLabel(dl, f.regular, kFontBody, ImVec2(x0, y), col::TextDim,
              "Выберите папку с GTA San Andreas. Установщик проверит её и поставит только то, чего не хватает.");
    y += S(44);

    float listW = l.size.x - l.margin * 2;
    if (!app.selectBanner.text.empty()) {
        ImGui::SetCursorScreenPos(ImVec2(x0, y));
        DrawBanner(app.selectBanner.severity, app.selectBanner.text.c_str(), nullptr, listW);
        y = ImGui::GetCursorScreenPos().y + S(4);
    }

    // Auto-select the first folder.
    if (app.selectedKey.empty() && !app.games.empty()) app.selectedKey = ToLower(app.games.front().dir.native());
    const GameEntry* selected = nullptr;
    bool searching = !d || !d->HintsDone() || d->ScanRunning();

    // Column header + hairline, then the scrollable rows.
    ColumnHeader(dl, ImVec2(x0, y), listW);
    y += S(24);
    HLine(dl, x0, x0 + listW, y, col::Line);
    y += S(1);

    float listH = l.origin.y + l.size.y - footerH - y - S(8);
    ImGui::SetCursorScreenPos(ImVec2(x0, y));
    ImGui::BeginChild("##games", ImVec2(listW + S(10), listH), ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    if (app.games.empty()) {
        EmptyState(searching, ImVec2(x0, y), listW, listH);
    } else {
        for (const GameEntry& e : app.games) {
            std::wstring key = ToLower(e.dir.native());
            int r = GameRow(e, key == app.selectedKey, listW);
            if (r) app.selectedKey = key;
            if (r == 2) {
                ImGui::PopStyleVar();
                ImGui::EndChild();
                app.OpenAnalysis(e.dir);
                return;
            }
        }
    }
    ImGui::PopStyleVar();
    ImGui::EndChild();
    for (const GameEntry& e : app.games)
        if (ToLower(e.dir.native()) == app.selectedKey) selected = &e;

    // Footer: search status on the left, actions on the right.
    float fy = BeginFooter(l, footerH);
    float by = fy + (footerH - S(36)) * 0.5f;
    float cy = fy + footerH * 0.5f;
    float ty = cy - TextSize(f.regular, kFontSmall, "Ag").y * 0.5f;
    if (d) {
        if (searching) {
            Spinner(dl, ImVec2(x0 + S(6), cy), S(5.5f), S(1.5f), col::TextDim);
            std::string status = d->HintsDone() ? std::format("Поиск на дисках · папок: {}", d->DirsScanned()) : "Ищем установленные игры…";
            DrawLabel(dl, f.regular, kFontSmall, ImVec2(x0 + S(20), ty), col::TextDim, status.c_str());
            if (d->ScanRunning()) {
                ImGui::SetCursorScreenPos(ImVec2(x0 + S(20) + TextSize(f.regular, kFontSmall, status.c_str()).x + S(14), ty));
                if (LinkButton("остановить##scan", kFontSmall, col::TextDim)) d->StopDiskScan();
            }
        } else {
            std::string status = std::format("Найдено: {} · поиск занял {:.1f} с", app.games.size(), d->ScanSeconds());
            DrawLabel(dl, f.regular, kFontSmall, ImVec2(x0, ty), col::TextFaint, status.c_str());
        }
    }
    float nextW = S(124), browseW = S(124);
    float rx = l.origin.x + l.size.x - l.margin;
    ImGui::SetCursorScreenPos(ImVec2(rx - nextW - S(8) - browseW, by));
    if (Button("Обзор…", ImVec2(browseW, S(36)), ButtonKind::Secondary)) app.BrowseFolder();
    ImGui::SetCursorScreenPos(ImVec2(rx - nextW, by));
    if (Button("Далее  " ICON_ARROW_RIGHT, ImVec2(nextW, S(36)), ButtonKind::Primary, selected != nullptr) && selected)
        app.OpenAnalysis(selected->dir);
}

}  // namespace uf::ui
