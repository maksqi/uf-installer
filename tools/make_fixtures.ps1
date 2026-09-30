<#
  Builds test "game folders" under out\fixtures from real installations (files are only COPIED):
    bare       - gta_sa.exe + SA-MP R1 + original vorbisFile.dll: everything must be installed
    r3-nosf    - SA-MP R3-1 without SAMPFUNCS: SAMPFUNCS 5.4.1 must NOT be installed
    full-r1    - complete R1 setup with "UltraFuck 2.4.lua": only the script is replaced
    cyrillic   - bare setup in a folder with Cyrillic letters and spaces
    arizona    - fake Arizona Games Launcher layout (<launcher>\bin\arizona) copied from the real one
#>
param(
    [string]$Gta = "$env:USERPROFILE\Desktop\GTA\GTA 140",
    [string]$Arizona = "$env:LOCALAPPDATA\Programs\Arizona Games Launcher\bin\arizona",
    [string]$Out = "$PSScriptRoot\..\out\fixtures"
)
$ErrorActionPreference = 'Stop'
if (Test-Path $Out) { Remove-Item -Recurse -Force $Out }
New-Item -ItemType Directory -Force $Out | Out-Null
$Out = (Resolve-Path $Out).Path

function New-Bare([string]$dir, [string]$sampFrom) {
    New-Item -ItemType Directory -Force $dir | Out-Null
    Copy-Item "$Gta\gta_sa.exe" $dir
    Copy-Item "$Gta\vorbisHooked.dll" "$dir\vorbisFile.dll"   # the original game DLL
    Copy-Item "$sampFrom\samp.dll" $dir
}

New-Bare "$Out\bare" $Gta
New-Bare "$Out\r3-nosf" $Arizona
New-Bare "$Out\Игры\GTA San Andreas (копия)" $Gta

$full = "$Out\full-r1"
New-Bare $full $Gta
foreach ($f in 'vorbisFile.dll', 'vorbisHooked.dll', 'CLEO.asi', 'SAMPFUNCS.asi', 'MoonLoader.asi', 'lua51.dll', 'bass.dll') {
    Copy-Item "$Gta\$f" $full -Force
}
Copy-Item "$Gta\scripts" $full -Recurse
Copy-Item "$Gta\cleo" $full -Recurse
Copy-Item "$Gta\moonloader" $full -Recurse
# keep the fixture small and deterministic: only the 2.4 script + libs + config
Get-ChildItem "$full\moonloader" -File | Where-Object { $_.Name -ne 'UltraFuck 2.4.lua' } | Remove-Item -Force
Get-ChildItem "$full\moonloader\config" -Directory | Where-Object { $_.Name -ne 'UltraFuck' } | Remove-Item -Recurse -Force

$launcher = "$Out\Arizona Games Launcher"
$az = "$launcher\bin\arizona"
New-Item -ItemType Directory -Force $az | Out-Null
New-Item -ItemType File "$launcher\Arizona Games Launcher.exe" | Out-Null
foreach ($f in 'gta_sa.exe', 'samp.dll', 'vorbisFile.dll', 'cleo.asi', 'SAMPFUNCS.asi', 'MoonLoader.asi', 'lua51.dll', 'bass.dll') {
    Copy-Item "$Arizona\$f" $az
}
Copy-Item "$Arizona\moonloader" $az -Recurse
Copy-Item "$Arizona\cleo" $az -Recurse
Get-ChildItem "$az\moonloader\config" -Directory -ErrorAction SilentlyContinue | Where-Object { $_.Name -ne 'UltraFuck' } | Remove-Item -Recurse -Force
Set-Content -Encoding utf8 "$Out\arizona-settings.json" '{"arizona":{"options":[{"id":"autoClean","value":true}]}}'

Get-ChildItem $Out -Directory | ForEach-Object { Write-Host "fixture: $($_.FullName)" }
