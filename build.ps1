<#
.SYNOPSIS
  Builds uf-installer.exe (x86, static CRT) inside the Visual Studio developer shell.
.EXAMPLE
  .\build.ps1                 # Release build + tests + import audit
  .\build.ps1 -Config Debug
  .\build.ps1 -Clean -Payload "D:\my\archives"
#>
param(
    [ValidateSet('Release', 'Debug')][string]$Config = 'Release',
    [switch]$Clean,
    [switch]$NoTests,
    [string]$Payload,                 # folder with the source archives (default: %USERPROFILE%\Desktop\uf-installer)
    [string]$Toolset = '14.44'        # MSVC v143: the last toolset whose static CRT still runs on Windows 7
)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw 'Visual Studio / Build Tools not found (vswhere.exe is missing).' }
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'No Visual Studio instance with the C++ x86/x64 tools.' }

if (-not $env:VSCMD_ARG_TGT_ARCH) {
    $env:PATH = "$(Split-Path $vswhere);$env:PATH"
    Import-Module "$vs\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
    $devArgs = "-arch=x86 -host_arch=x64 -no_logo"
    if ($Toolset) { $devArgs += " -vcvars_ver=$Toolset" }
    Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments $devArgs | Out-Null
}
if ($env:VSCMD_ARG_TGT_ARCH -ne 'x86') { throw "Developer shell targets '$env:VSCMD_ARG_TGT_ARCH', expected x86. Run build.ps1 from a fresh PowerShell." }

# MinGW/MSYS on PATH would confuse CMake's compiler detection.
$env:PATH = ($env:PATH -split ';' | Where-Object { $_ -and $_ -notmatch 'w64devkit|mingw|msys64' }) -join ';'

$preset = "x86-$($Config.ToLower())"
$buildDir = Join-Path $root "build\$preset"
if ($Clean -and (Test-Path $buildDir)) { Remove-Item -Recurse -Force $buildDir }

$configureArgs = @('--preset', $preset)
if ($Payload) { $configureArgs += "-DUF_PAYLOAD_SRC=$Payload" }
Push-Location $root
try {
    & cmake @configureArgs
    if ($LASTEXITCODE) { throw 'CMake configure failed' }
    & cmake --build --preset $preset
    if ($LASTEXITCODE) { throw 'Build failed' }
    if (-not $NoTests) {
        & ctest --preset $preset
        if ($LASTEXITCODE) { throw 'Tests failed' }
        # Every T()/F() text needs a Ukrainian entry in src/core/i18n_uk.cpp.
        & python "$root\tools\i18n_check.py"
        if ($LASTEXITCODE) { throw 'Ukrainian translation is incomplete' }
    }
    $exe = Join-Path $buildDir 'uf-installer.exe'
    if (Test-Path $exe) {
        & "$root\tools\audit_imports.ps1" -Exe $exe
        if ($LASTEXITCODE) { throw 'Import audit failed' }
        $size = [Math]::Round((Get-Item $exe).Length / 1MB, 1)
        Write-Host "`nOK: $exe ($size MB)" -ForegroundColor Green
    }
} finally {
    Pop-Location
}
