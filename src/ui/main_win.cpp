// UltraFuck installer - Win32 window, Direct3D 9 + Dear ImGui main loop.
#include <windows.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <windowsx.h>

#include <imgui.h>
#include <imgui_impl_dx9.h>
#include <imgui_impl_win32.h>

#include "core/args.h"
#include "core/elevation.h"
#include "core/i18n.h"
#include "core/installer.h"
#include "core/log.h"
#include "resource.h"
#include "ui/app.h"
#include "ui/d3d9_device.h"
#include "ui/theme.h"
#include "ui/widgets.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace uf::ui {

namespace {

constexpr wchar_t kWindowClass[] = L"UltraFuckSetupWindow";
constexpr wchar_t kMutexName[] = L"Local\\UltraFuckSetup.Instance";

HANDLE g_mutex = nullptr;
App* g_app = nullptr;
D3D9Device g_device;
UINT g_resizeW = 0, g_resizeH = 0;
float g_pendingScale = 0.f;

// Single instance: a second normal launch activates the existing window; the elevated
// copy waits until the non-elevated one has handed over and exited.
bool AcquireInstanceLock(bool waitForOwner) {
    g_mutex = CreateMutexW(nullptr, FALSE, kMutexName);
    if (!g_mutex) return GetLastError() != ERROR_ACCESS_DENIED;
    DWORD r = WaitForSingleObject(g_mutex, waitForOwner ? 8000 : 0);
    if (r == WAIT_OBJECT_0 || r == WAIT_ABANDONED) return true;
    if (HWND other = FindWindowW(kWindowClass, nullptr)) {
        if (IsIconic(other)) ShowWindow(other, SW_RESTORE);
        SetForegroundWindow(other);
    }
    CloseHandle(g_mutex);
    g_mutex = nullptr;
    return false;
}

LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp)) return TRUE;
    switch (msg) {
        case WM_NCCALCSIZE:
            if (wp == TRUE) return 0;  // the whole window is client area (custom title bar)
            break;
        case WM_NCHITTEST: {
            POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            ScreenToClient(hwnd, &pt);
            RECT rc;
            GetClientRect(hwnd, &rc);
            if (!PtInRect(&rc, pt)) return HTNOWHERE;
            return g_app && g_app->CaptionHit(pt, rc) ? HTCAPTION : HTCLIENT;
        }
        case WM_NCACTIVATE:
            return DefWindowProcW(hwnd, msg, wp, -1);  // do not paint a classic frame
        case WM_SIZE:
            if (wp != SIZE_MINIMIZED) {
                g_resizeW = LOWORD(lp);
                g_resizeH = HIWORD(lp);
            }
            return 0;
        case WM_DPICHANGED: {
            g_pendingScale = HIWORD(wp) / 96.f;
            const RECT* r = reinterpret_cast<const RECT*>(lp);
            SetWindowPos(hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
        case WM_SETTINGCHANGE:
            // Windows switched between the light and the dark app mode.
            if (g_app && lp && lstrcmpW(reinterpret_cast<LPCWSTR>(lp), L"ImmersiveColorSet") == 0) g_app->OnSystemThemeChanged();
            break;
        case WM_SYSCOMMAND:
            if ((wp & 0xFFF0) == SC_KEYMENU) return 0;
            break;
        case WM_CLOSE:
            if (g_app && !g_app->CanClose()) {
                MessageBeep(MB_ICONWARNING);
                return 0;
            }
            break;
        case WM_ERASEBKGND:
            return 1;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_APP_WAKE:
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void ApplyWindowChrome(HWND hwnd) {
    MARGINS margins{0, 0, 1, 0};  // keeps the DWM drop shadow on the borderless window
    DwmExtendFrameIntoClientArea(hwnd, &margins);
    BOOL dark = CurrentTheme() == Theme::Dark;
    DwmSetWindowAttribute(hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &dark, sizeof(dark));
    int corners = 2;  // DWMWCP_ROUND (Windows 11)
    DwmSetWindowAttribute(hwnd, 33 /*DWMWA_WINDOW_CORNER_PREFERENCE*/, &corners, sizeof(corners));
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

int Run(HINSTANCE instance) {
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    Args args = ParseArgs(argc, argv);
    LocalFree(argv);

    log::Init(GetFolder(Folder::Temp) / L"uf-installer.log");
    log::Info("uf-installer started{}{}", args.elevated ? " (elevated relaunch)" : "", IsProcessElevated() ? " [admin]" : "");
    for (const std::wstring& u : args.unknown) log::Warn("Unknown argument: {}", ToUtf8(u));

    // Language and theme: from the command line (elevated relaunch keeps what the user saw), else from Windows.
    SetLang(ParseLang(args.lang).value_or(SystemLang()));
    SetTheme(ParseTheme(args.theme).value_or(SystemTheme()));
    log::Info("Language {}, theme {}", LangCode(CurrentLang()), ThemeName(CurrentTheme()));

    // Screenshot runs are for development and may run next to a normal window.
    if (!args.screenshot && !AcquireInstanceLock(args.elevated)) {
        log::Info("Another instance is running - activated it");
        return 0;
    }
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    CleanupStaleTempDirs();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.hIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED));
    wc.hIconSm = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                                               GetSystemMetrics(SM_CYSMICON), LR_SHARED));
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(CurrentTheme() == Theme::Dark ? RGB(12, 12, 12) : RGB(255, 255, 255));
    wc.lpszClassName = kWindowClass;
    RegisterClassExW(&wc);

    // Place the window on the monitor under the cursor (or where the non-elevated copy was).
    POINT anchor{};
    if (args.pos)
        anchor = {args.pos->first + 40, args.pos->second + 40};
    else
        GetCursorPos(&anchor);
    HMONITOR monitor = MonitorFromPoint(anchor, MONITOR_DEFAULTTOPRIMARY);
    float scale = (args.dpiScale > 0 ? args.dpiScale : ImGui_ImplWin32_GetDpiScaleForMonitor(monitor)) * kUiZoom;
    SetUiScale(scale);
    MONITORINFO mi{sizeof(mi)};
    GetMonitorInfoW(monitor, &mi);
    int w = static_cast<int>(kWindowWidth * scale), h = static_cast<int>(kWindowHeight * scale);
    const RECT& work = mi.rcWork;
    w = std::min<int>(w, work.right - work.left);
    h = std::min<int>(h, work.bottom - work.top);
    int x = args.pos ? args.pos->first : work.left + (work.right - work.left - w) / 2;
    int y = args.pos ? args.pos->second : work.top + (work.bottom - work.top - h) / 2;

    HWND hwnd = CreateWindowExW(WS_EX_APPWINDOW, kWindowClass, ToWide(WindowTitle()).c_str(), WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                x, y, w, h, nullptr, nullptr, instance, nullptr);
    if (!hwnd) return 1;
    ApplyWindowChrome(hwnd);

    RECT client{};
    GetClientRect(hwnd, &client);
    if (!g_device.Create(hwnd, static_cast<UINT>(client.right), static_cast<UINT>(client.bottom))) {
        log::Error("Direct3D 9 is not available");
        std::string text = T("Не удалось запустить графику (Direct3D 9).\nОбновите драйвер видеокарты и попробуйте снова.\n\n"
                             "Лог: %TEMP%\\uf-installer.log",
                             "Could not start the graphics (Direct3D 9).\nUpdate the video card driver and try again.\n\n"
                             "Log: %TEMP%\\uf-installer.log");
        MessageBoxW(nullptr, ToWide(text).c_str(), ToWide(WindowTitle()).c_str(), MB_ICONERROR);
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    LoadFonts();
    ApplyStyle(scale);
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX9_Init(g_device.device());

    App app(hwnd, args);
    g_app = &app;
    app.Start();

    ShowWindow(hwnd, SW_SHOWNORMAL);
    UpdateWindow(hwnd);
    SetForegroundWindow(hwnd);

    const ULONGLONG started = GetTickCount64();
    bool screenshotTaken = false;
    int framesLeft = 3;
    bool running = true;
    while (running) {
        if (app.QuitRequested()) {
            DestroyWindow(hwnd);
            break;
        }
        bool animate = framesLeft > 0 || app.Animating() || (args.screenshot && !screenshotTaken);
        if (!animate || IsIconic(hwnd)) WaitMessage();
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) running = false;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            framesLeft = 6;
        }
        if (!running) break;
        if (app.QuitRequested()) {
            // Leave right away: WaitMessage() would not wake up for the WM_QUIT posted by WM_DESTROY.
            DestroyWindow(hwnd);
            break;
        }
        if (IsIconic(hwnd)) continue;

        if (g_pendingScale > 0.f) {
            scale = (args.dpiScale > 0 ? args.dpiScale : g_pendingScale) * kUiZoom;
            g_pendingScale = 0.f;
            SetUiScale(scale);
            ApplyStyle(scale);
        }
        if (g_resizeW && g_resizeH) {
            g_device.Resize(g_resizeW, g_resizeH);
            g_resizeW = g_resizeH = 0;
        }
        if (!g_device.Ready()) {
            Sleep(20);
            continue;
        }

        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        app.Frame();
        ImGui::EndFrame();
        ImGui::Render();
        ImVec4 bg = ImGui::ColorConvertU32ToFloat4(col::Bg);
        g_device.Render(ImGui::GetDrawData(), D3DCOLOR_COLORVALUE(bg.x, bg.y, bg.z, 1.f));
        if (args.screenshot && !screenshotTaken && GetTickCount64() - started >= static_cast<ULONGLONG>(args.screenshotDelayMs) &&
            app.ReadyForScreenshot()) {
            screenshotTaken = true;
            g_device.SaveBackbufferPng(*args.screenshot);
            app.RequestQuit();
        }
        g_device.Present();
        if (framesLeft > 0) --framesLeft;
    }

    g_app = nullptr;
    ImGui_ImplDX9_Shutdown();
    ReleaseTextures();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    g_device.Destroy();
    CoUninitialize();
    log::Info("uf-installer exited");
    return 0;
}

}  // namespace

void ReleaseInstanceLock() {
    if (g_mutex) {
        ReleaseMutex(g_mutex);
        CloseHandle(g_mutex);
        g_mutex = nullptr;
    }
}

}  // namespace uf::ui

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) { return uf::ui::Run(instance); }
