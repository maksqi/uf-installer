#pragma once
#include <functional>
#include <stop_token>
#include <string>

#include "core/paths.h"

namespace uf {

inline constexpr wchar_t kDxWebSetupUrl[] =
    L"https://download.microsoft.com/download/1/7/1/1718ccc4-6315-4d8e-9543-8e28a4e18c4c/dxwebsetup.exe";
inline constexpr char kDxDownloadPage[] = "https://www.microsoft.com/download/details.aspx?id=35";

// d3dx9_43.dll (32-bit) next to the game or in SysWOW64/System32 - where gta_sa.exe will look for it.
bool HasD3DX9_43(const fs::path& gameDir);

struct DxResult {
    bool ok = false;
    std::string message;
};

// Downloads dxwebsetup.exe into `workDir`, checks the Microsoft signature and runs it silently (/Q).
DxResult InstallDirectX(const fs::path& workDir, const std::function<void(const std::string&)>& status, std::stop_token stop);

// WinVerifyTrust + signer name check ("Microsoft Corporation").
bool VerifyMicrosoftSignature(const fs::path& file, std::string* signer);

}  // namespace uf
