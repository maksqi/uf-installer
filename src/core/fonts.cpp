#include "core/fonts.h"

#include <windows.h>

#include <format>

#include "core/fsutil.h"
#include "core/log.h"
#include "core/registry.h"

namespace uf {

fs::path SystemFontsDir() { return GetFolder(Folder::Fonts); }

void InstallFontFile(const fs::path& staged, const fs::path& fontsDir, const std::string& file, const std::string& regName,
                     bool registerSystem) {
    CreateDirs(fontsDir);
    fs::path dest = fontsDir / ToWide(file);
    if (!FileExists(dest)) CopyFileRetry(staged, dest);
    if (!registerSystem) return;

    // The Fonts key is shared between the 32- and 64-bit registry views.
    LONG r = reg::WriteString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts",
                              ToWide(regName).c_str(), ToWide(file), KEY_WOW64_64KEY);
    if (r != ERROR_SUCCESS)
        throw FsError(static_cast<unsigned long>(r), std::format("Не удалось зарегистрировать шрифт {}: {}", file, Win32ErrorText(r)));
    if (AddFontResourceW(dest.c_str()) == 0) log::Warn("AddFontResource({}) failed", file);
}

void BroadcastFontChange() {
    DWORD_PTR result = 0;
    SendMessageTimeoutW(HWND_BROADCAST, WM_FONTCHANGE, 0, 0, SMTO_ABORTIFHUNG, 1000, &result);
}

}  // namespace uf
