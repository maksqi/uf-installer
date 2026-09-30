#include "core/directx.h"

#include <windows.h>
#include <shellapi.h>
#include <softpub.h>
#include <urlmon.h>
#include <wincrypt.h>
#include <wintrust.h>

#include <format>

#include "core/fsutil.h"
#include "core/log.h"
#include "core/pe.h"

namespace uf {

bool HasD3DX9_43(const fs::path& gameDir) {
    if (pe::IsI386(gameDir / L"d3dx9_43.dll")) return true;
    wchar_t sys[MAX_PATH];
    // 32-bit process on 64-bit Windows: SysWOW64 holds the x86 DLLs. On 32-bit Windows this call fails.
    UINT n = GetSystemWow64DirectoryW(sys, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) n = GetSystemDirectoryW(sys, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return false;
    return pe::IsI386(fs::path(std::wstring(sys, n)) / L"d3dx9_43.dll");
}

bool VerifyMicrosoftSignature(const fs::path& file, std::string* signer) {
    WINTRUST_FILE_INFO fileInfo{};
    fileInfo.cbStruct = sizeof(fileInfo);
    fileInfo.pcwszFilePath = file.c_str();

    WINTRUST_DATA data{};
    data.cbStruct = sizeof(data);
    data.dwUIChoice = WTD_UI_NONE;
    data.fdwRevocationChecks = WTD_REVOKE_NONE;
    data.dwUnionChoice = WTD_CHOICE_FILE;
    data.pFile = &fileInfo;
    data.dwStateAction = WTD_STATEACTION_VERIFY;

    GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    LONG status = WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &action, &data);

    std::wstring name;
    if (status == ERROR_SUCCESS) {
        if (CRYPT_PROVIDER_DATA* provider = WTHelperProvDataFromStateData(data.hWVTStateData)) {
            if (CRYPT_PROVIDER_SGNR* sgnr = WTHelperGetProvSignerFromChain(provider, 0, FALSE, 0)) {
                if (CRYPT_PROVIDER_CERT* cert = WTHelperGetProvCertFromChain(sgnr, 0); cert && cert->pCert) {
                    wchar_t buf[256] = {};
                    CertGetNameStringW(cert->pCert, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, buf, 256);
                    name = buf;
                }
            }
        }
    }
    data.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &action, &data);

    if (signer) *signer = ToUtf8(name);
    return status == ERROR_SUCCESS && name == L"Microsoft Corporation";
}

DxResult InstallDirectX(const fs::path& workDir, const std::function<void(const std::string&)>& status, std::stop_token stop) {
    auto say = [&](const std::string& s) {
        log::Info("{}", s);
        if (status) status(s);
    };
    CreateDirs(workDir);
    fs::path exe = workDir / L"dxwebsetup.exe";
    say("Скачивание веб-установщика DirectX с сайта Microsoft…");
    HRESULT hr = URLDownloadToFileW(nullptr, kDxWebSetupUrl, exe.c_str(), 0, nullptr);
    if (FAILED(hr) || !FileExists(exe))
        return {false, std::format("Не удалось скачать DirectX (ошибка 0x{:08X}). Проверьте интернет или скачайте вручную: {}",
                                   static_cast<unsigned>(hr), kDxDownloadPage)};

    std::string signer;
    if (!VerifyMicrosoftSignature(exe, &signer))
        return {false, std::format("Скачанный файл не прошёл проверку подписи Microsoft (подписант: «{}»). Установка отменена.",
                                   signer.empty() ? "нет" : signer)};

    say("Установка DirectX, это может занять несколько минут…");
    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
    sei.lpVerb = L"open";
    sei.lpFile = exe.c_str();
    sei.lpParameters = L"/Q";
    sei.nShow = SW_HIDE;
    if (!ShellExecuteExW(&sei) || !sei.hProcess) {
        DWORD e = GetLastError();
        return {false, std::format("Не удалось запустить установщик DirectX: {}", Win32ErrorText(e))};
    }
    const ULONGLONG deadline = GetTickCount64() + 15ull * 60 * 1000;
    DWORD wait = WAIT_TIMEOUT;
    while (wait == WAIT_TIMEOUT && GetTickCount64() < deadline && !stop.stop_requested())
        wait = WaitForSingleObject(sei.hProcess, 250);
    DWORD code = 1;
    GetExitCodeProcess(sei.hProcess, &code);
    CloseHandle(sei.hProcess);
    if (wait != WAIT_OBJECT_0) return {false, "Установка DirectX не завершилась вовремя — дождитесь её окончания и запустите установщик снова."};
    if (code != 0) log::Warn("dxwebsetup exit code {}", code);
    return {true, code == 0 ? "DirectX установлен." : std::format("Установщик DirectX завершился с кодом {}.", code)};
}

}  // namespace uf
