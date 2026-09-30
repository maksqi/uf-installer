<#
  Checks that the exe is self-contained and loads on Windows 7:
  x86, no MSVC runtime DLLs, no api-ms-win-* sets, no d3dcompiler, no Windows 8+ only imports.
  Must run inside the VS developer shell (needs dumpbin).
#>
param([Parameter(Mandatory)][string]$Exe)
$ErrorActionPreference = 'Stop'

$headers = & dumpbin /nologo /headers $Exe
$imports = & dumpbin /nologo /imports $Exe
$problems = @()

if (-not ($headers -match 'machine \(x86\)')) { $problems += 'not an x86 image' }
$subsys = ($headers | Select-String 'subsystem version' | Select-Object -First 1).ToString().Trim()
if ($subsys -notmatch '^(5\.\d+|6\.0[01]) subsystem version') { $problems += "subsystem version too high: $subsys" }

$dlls = $imports | Where-Object { $_ -match '^\s{4}\S+\.(dll|DLL)$' } | ForEach-Object { $_.Trim().ToLower() }
foreach ($d in $dlls) {
    if ($d -match '^(vcruntime|msvcp|ucrtbase|concrt)') { $problems += "MSVC runtime dependency: $d" }
    if ($d -match '^api-ms-win-') { $problems += "API set dependency: $d" }
    if ($d -match '^d3dcompiler') { $problems += "d3dcompiler dependency: $d" }
}

$win8Only = 'GetSystemTimePreciseAsFileTime', 'WaitOnAddress', 'WakeByAddressSingle', 'WakeByAddressAll', 'CreateFile2',
            'GetDpiForWindow', 'GetDpiForSystem', 'SetThreadDescription', 'SetProcessDpiAwarenessContext',
            'GetCurrentPackageId', 'SetThreadpoolTimerEx', 'CopyFile2', 'GetDpiForMonitor', 'SetProcessDpiAwareness',
            'AdjustWindowRectExForDpi', 'EnableNonClientDpiScaling', 'PrefetchVirtualMemory'
foreach ($f in $win8Only) {
    if ($imports -match "^\s+[0-9A-F]+\s+$f$") { $problems += "Windows 8+ import: $f" }
}

Write-Host "Imported DLLs: $($dlls -join ', ')"
Write-Host $subsys
if ($problems) {
    $problems | ForEach-Object { Write-Host "AUDIT: $_" -ForegroundColor Red }
    exit 1
}
Write-Host 'Import audit passed (x86, static CRT, Windows 7 compatible imports).' -ForegroundColor Green
exit 0
