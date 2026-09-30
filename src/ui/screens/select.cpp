#include <format>

#include "core/versions.h"
#include "payload_manifest.gen.h"
#include "ui/IconsFontAwesome6.h"
#include "ui/screens/screens.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace uf::ui {

namespace {

struct Chip {
    std::string text;
    ImU32 color;
};

std::vector<Chip> BadgesFor(const GameEntry& e) {
    std::vector<Chip> out;
    if (!e.arizonaId.empty()) out.push_back({"Arizona Launcher", col::Accent});
    const FolderReport* r = e.report.get();
    if (!r) return out;
    if (!r->hasSamp)
        out.push_back({"нет SA-MP", col::Err});
    else
        out.push_back({"SA-MP " + std::string(SampShortName(r->samp)), col::Info});
    std::string ufVersion;
    bool same = r->ufExactInstalled;
    for (const UfScript& s : r->ufScripts) {
        if (!s.loadable) continue;
        if (ufVersion.empty()) ufVersion = s.version.empty() ? "?" : s.version;
        if (!s.version.empty() && CompareDecimalVersions(s.version, gen::kScriptVersion) == 0) same = true;
    }
    if (same)
        out.push_back({std::string("UltraFuck ") + gen::kScriptVersion, col::Ok});
    else if (!ufVersion.empty())
        out.push_back({"UltraFuck " + ufVersion, col::Warn});
    return out;
}

struct Component {
    const char* name;
    bool ok;
};

std::vector<Component> ComponentsOf(const FolderReport& r) {
    bool fonts = true;
    for (const FontState& f : r.fonts)
        if (f.bundled && f.required && !f.present) fonts = false;
    return {{"ASI Loader", r.loader != LoaderKind::None && r.loader != LoaderKind::Missing},
            {"CLEO", r.cleo.has_value()},
            {"SAMPFUNCS", r.sampfuncs.has_value()},
            {"MoonLoader", r.moonloader.has_value()},
            {"Шрифты", fonts},
            {"DirectX", r.d3dx9}};
}

// Returns 1 on click, 2 on double click.
int GameCard(const GameEntry& e, bool selected, float width) {
    Fonts& f = GetFonts();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float h = S(88);
    ImGui::PushID(PathUtf8(e.dir).c_str());
    bool clicked = ImGui::InvisibleButton("##card", ImVec2(width, h));
    bool hovered = ImGui::IsItemHovered();
    bool dbl = hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
    ImGui::PopID();
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 q(p.x + width, p.y + h);
    dl->AddRectFilled(p, q, selected ? WithAlpha(col::Accent, 0.10f) : hovered ? col::SurfaceHover : col::Surface, S(12));
    dl->AddRect(p, q, selected ? col::Accent : hovered ? col::Border : WithAlpha(col::Border, 0.7f), S(12), S(selected ? 1.5f : 1.f));

    bool az = !e.arizonaId.empty();
    IconCircle(dl, ImVec2(p.x + S(42), p.y + h * 0.5f), S(23), az ? col::Accent : col::Info, az ? ICON_FA_ROCKET : ICON_FA_GAMEPAD, 18.f);

    float x = p.x + S(80);
    float right = q.x - S(44);
    float y1 = p.y + S(14);
    DrawLabel(dl, f.bold, kFontTitle, ImVec2(x, y1), col::Text, e.title.c_str());
    float bx = x + TextSize(f.bold, kFontTitle, e.title.c_str()).x + S(10);
    for (const Chip& c : BadgesFor(e)) {
        if (bx + BadgeWidth(c.text.c_str()) > right) break;
        bx += Badge(dl, ImVec2(bx, y1 - S(1)), c.text.c_str(), c.color) + S(6);
    }

    std::string path = EllipsizeLeft(f.regular, kFontSmall, PathUtf8(e.dir), right - x);
    DrawLabel(dl, f.regular, kFontSmall, ImVec2(x, p.y + S(38)), col::TextDim, path.c_str());

    float y3 = p.y + S(61);
    if (!e.report) {
        Spinner(dl, ImVec2(x + S(6), y3 + S(8)), S(6), S(2), col::TextDim);
        DrawLabel(dl, f.regular, kFontTiny, ImVec2(x + S(20), y3), col::TextFaint, "Проверяем папку…");
    } else {
        float cx = x;
        for (const Component& c : ComponentsOf(*e.report)) {
            const char* icon = c.ok ? ICON_FA_CHECK : ICON_FA_XMARK;
            DrawLabel(dl, f.regular, kFontTiny, ImVec2(cx, y3), c.ok ? col::Ok : col::Err, icon);
            cx += TextSize(f.regular, kFontTiny, icon).x + S(4);
            DrawLabel(dl, f.regular, kFontTiny, ImVec2(cx, y3), c.ok ? col::TextDim : col::Text, c.name);
            cx += TextSize(f.regular, kFontTiny, c.name).x + S(16);
        }
    }
    ImVec2 cs = TextSize(f.regular, kFontBody, ICON_FA_CHEVRON_RIGHT);
    DrawLabel(dl, f.regular, kFontBody, ImVec2(q.x - S(26), p.y + (h - cs.y) * 0.5f), hovered || selected ? col::Text : col::TextFaint,
             ICON_FA_CHEVRON_RIGHT);
    return dbl ? 2 : clicked ? 1 : 0;
}

void EmptyState(bool searching, float width, float height) {
    Fonts& f = GetFonts();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 c(p.x + width * 0.5f, p.y + height * 0.38f);
    const char* title = searching ? "Ищем GTA San Andreas…" : "GTA San Andreas не найдена";
    const char* text = searching ? "Проверяем лаунчеры, реестр и диски компьютера."
                                 : "Нажмите «Обзор…» и укажите папку с игрой — ту, где лежит gta_sa.exe.";
    if (searching)
        Spinner(dl, c, S(22), S(3.5f), col::Accent);
    else
        IconCircle(dl, c, S(30), col::Muted, ICON_FA_MAGNIFYING_GLASS, 22.f);
    ImVec2 ts = TextSize(f.bold, 17.f, title);
    DrawLabel(dl, f.bold, 17.f, ImVec2(c.x - ts.x * 0.5f, c.y + S(44)), col::Text, title);
    ImVec2 ds = TextSize(f.regular, kFontSmall, text);
    DrawLabel(dl, f.regular, kFontSmall, ImVec2(c.x - ds.x * 0.5f, c.y + S(72)), col::TextDim, text);
}

}  // namespace

void DrawSelectScreen(App& app) {
    Layout l = GetLayout();
    Fonts& f = GetFonts();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float footerH = S(76);
    Discovery* d = app.discovery();

    float y = l.top + S(22);
    DrawLabel(dl, f.bold, kFontHeading, ImVec2(l.origin.x + l.margin, y), col::Text, "Куда установить UltraFuck?");
    y += S(36);
    DrawLabel(dl, f.regular, kFontBody, ImVec2(l.origin.x + l.margin, y), col::TextDim,
             "Выберите папку с GTA San Andreas — установщик проверит, чего не хватает, и поставит только нужное.");
    y += S(34);

    ImGui::SetCursorScreenPos(ImVec2(l.origin.x + l.margin, y));
    float listW = l.size.x - l.margin * 2;
    if (!app.selectBanner.text.empty()) {
        DrawBanner(app.selectBanner.severity, app.selectBanner.text.c_str(), nullptr, listW);
        y = ImGui::GetCursorScreenPos().y;
    }

    // Auto-select the first folder.
    if (app.selectedKey.empty() && !app.games.empty()) app.selectedKey = ToLower(app.games.front().dir.native());
    const GameEntry* selected = nullptr;

    float listH = l.origin.y + l.size.y - footerH - y - S(12);
    ImGui::SetCursorScreenPos(ImVec2(l.origin.x + l.margin, y));
    ImGui::BeginChild("##games", ImVec2(listW + S(16), listH), ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
    bool searching = !d || !d->HintsDone() || d->ScanRunning();
    if (app.games.empty()) {
        EmptyState(searching, listW, listH);
    } else {
        for (const GameEntry& e : app.games) {
            std::wstring key = ToLower(e.dir.native());
            bool isSel = key == app.selectedKey;
            int r = GameCard(e, isSel, listW);
            if (r) app.selectedKey = key;
            if (r == 2) {
                ImGui::EndChild();
                app.OpenAnalysis(e.dir);
                return;
            }
            ImGui::Dummy(ImVec2(0, S(2)));
        }
    }
    ImGui::EndChild();
    for (const GameEntry& e : app.games)
        if (ToLower(e.dir.native()) == app.selectedKey) selected = &e;

    // Footer
    float fy = BeginFooter(l, footerH);
    float by = fy + (footerH - S(40)) * 0.5f;
    float sx = l.origin.x + l.margin;
    if (d) {
        std::string status;
        if (searching) {
            Spinner(dl, ImVec2(sx + S(8), fy + footerH * 0.5f), S(7), S(2.2f), col::Accent);
            status = d->HintsDone() ? std::format("Поиск на дисках… просмотрено папок: {}", d->DirsScanned()) : "Ищем установленные игры…";
            DrawLabel(dl, f.regular, kFontSmall, ImVec2(sx + S(24), fy + footerH * 0.5f - S(9)), col::TextDim, status.c_str());
            if (d->ScanRunning()) {
                float tw = TextSize(f.regular, kFontSmall, status.c_str()).x;
                ImGui::SetCursorScreenPos(ImVec2(sx + S(34) + tw, by + S(4)));
                if (Button("Остановить##scan", ImVec2(S(110), S(32)), ButtonKind::Ghost)) d->StopDiskScan();
            }
        } else {
            status = std::format("{} Найдено папок с игрой: {} · поиск занял {:.1f} с", ICON_FA_CIRCLE_CHECK, app.games.size(), d->ScanSeconds());
            DrawLabel(dl, f.regular, kFontSmall, ImVec2(sx, fy + footerH * 0.5f - S(9)), col::TextDim, status.c_str());
        }
    }
    float nextW = S(150), browseW = S(140);
    float rx = l.origin.x + l.size.x - l.margin;
    ImGui::SetCursorScreenPos(ImVec2(rx - nextW - S(10) - browseW, by));
    if (Button(ICON_FA_FOLDER_OPEN "  Обзор…", ImVec2(browseW, S(40)), ButtonKind::Secondary)) app.BrowseFolder();
    ImGui::SetCursorScreenPos(ImVec2(rx - nextW, by));
    if (Button("Далее  " ICON_FA_CHEVRON_RIGHT, ImVec2(nextW, S(40)), ButtonKind::Primary, selected != nullptr) && selected)
        app.OpenAnalysis(selected->dir);
}

}  // namespace uf::ui
