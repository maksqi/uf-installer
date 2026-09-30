# UltraFu#k Setup

**English** · [Русский](README.ru.md)

Installer for the **UltraFu#k** MoonLoader script for GTA San Andreas (SA-MP). It is a single `.exe` of about 11 MB with every required file embedded.

**[Download uf-installer.exe](https://github.com/maksqi/uf-installer/releases/latest/download/uf-installer.exe)** (latest version; all versions are on the [Releases](https://github.com/maksqi/uf-installer/releases) page). Runs on Windows 7–11, nothing else to install.

What the installer does:

- **Finds the game.** Locates every folder with `gta_sa.exe`. In about 0.1 s it checks the SA-MP registry key, **Arizona Games Launcher** (`settings.json` → `gamePath` or `<launcher>\bin\<id>`), the launch history, shortcuts and installed programs. Then it scans all local drives in parallel within a few seconds. You can also pick a folder by hand.
- **Inspects the chosen folder.** It detects:
  - the `gta_sa.exe` version;
  - the SA-MP build (by the `samp.dll` entry point);
  - the ASI loader: Silent, Ultimate or the Arizona one;
  - CLEO, SAMPFUNCS, MoonLoader and their versions;
  - MoonLoader libraries;
  - installed UltraFu#k versions;
  - `d3dx9_43.dll` and the Trebuchet MS fonts (`imgui.lua` crashes without `trebucbd.ttf`).
- **Installs only what is missing.** Files are unpacked to `%TEMP%\uf-installer-*` first. Every replaced file is saved to `<game>\uf-installer-backup\<date>\`. Any error rolls all changes back.
- **Leaves alone what Arizona Launcher manages**: the loader, CLEO, SAMPFUNCS, `bass.dll`.
- **Keeps settings and old versions.** It installs the UltraFu#k version embedded in the exe and moves other versions (for example `UltraFu#k 2.4.lua`) to the backup. `Settings.ini` with the login is never overwritten.
- **Asks for administrator rights only when needed**: fonts in `C:\Windows\Fonts`, DirectX, a game folder without write access. Then the installer restarts itself through UAC.
- **Updates MoonLoader libraries.** The "Overwrite all libraries" option is on by default: libraries that differ from the bundled ones are replaced (the old ones go to the backup), identical ones are left as they are.
- **Light and dark theme; Russian, Ukrainian and English.** The theme follows the Windows setting and changes with it. Language: Ukrainian on Ukrainian Windows, Russian on Russian and other CIS languages, English otherwise. Both can be switched in the window title bar (the language through a menu with flags).

## Building

You need Visual Studio 2022/2026 Build Tools (C++ x86, toolset **14.44**) and Python 3.

```powershell
.\build.ps1                 # Release: build + tests + import audit (Windows 7+)
.\build.ps1 -Clean          # from scratch
.\build.ps1 -Payload D:\uf  # archives in another folder
```

Output: `build\x86-release\uf-installer.exe`.

The files to install are in the `payload/` folder:

| Archive | Contents |
|---|---|
| `silents_asi_loader_13.zip` | ASI Loader |
| `CLEO.zip` | CLEO 4.3.22 |
| `SAMPFUNCS_5.4.1.zip` | SAMPFUNCS (SA-MP R1 only) |
| `moonloader_0.26.zip` | MoonLoader 026.5 |
| `UltraFu#k_2.36.zip` | the script, `config/`, `lib/` |
| `trebuchet_ms.zip` | Trebuchet MS fonts |

The archives are compressed with LZMA at the strongest settings (preset 9e). Inside the exe the files are one LZMA archive as well. WinRAR and 7-Zip open these zips; Windows Explorer does not.

**Updating the script:** replace the script archive in `payload/` with the new `<name>_<version>.zip` and run `.\build.ps1`. The version comes from the archive name. The packer `tools/pack_payload.py` works out what each archive contains, sorts the files into components and resolves conflicts between archives. Its report is `build\x86-release\generated\payload_report.txt`.

**Recompressing an archive:** `python tools/repack_zip.py payload\<archive>.zip`. To pack a folder: `python tools/repack_zip.py <folder> -o payload\<archive>.zip`. The packer accepts only `.zip`, so unpack a `.rar` or `.7z` first and pack it with this command.

**UI texts** are written in the code as pairs `T("русский", "english")`. The Ukrainian translation is in `src/core/i18n_uk.cpp`, keyed by the Russian text. `tools/i18n_check.py` runs during the build and fails if any text lacks a translation.

**Changing the icon:** run `.\tools\make_icon.ps1 -Source <image.png>` (square, 256 px or larger). It rebuilds `res/app.ico` with 16–256 px sizes and rounded corners. The same icon is shown in the window title bar.

## Releases

GitHub Actions (`.github/workflows/build.yml`) builds the exe on every push and pull request. The run includes the tests, the translation check and the import audit, and keeps the exe as a run artifact.

To release a version, bump `VERSION` in `CMakeLists.txt`, commit, and push a tag:

```powershell
git tag v1.0.1
git push origin v1.0.1
```

The workflow checks that the tag matches the version, builds the exe and publishes it on the Releases page. The Download link above always points to the latest release.

## Layout

```
src/core/   folder inspection, plan, installation, game search, Arizona, fonts, DirectX, UAC (no UI)
src/ui/     Win32 window + Direct3D 9Ex + Dear ImGui 1.92 (monochrome theme, custom title bar, per-monitor DPI)
tests/      doctest: unit tests + install/rollback in a real temporary folder
tools/      payload packer, archive recompression, icon generator, fixtures, import audit
payload/    archives with the files to install
```

Libraries: Dear ImGui 1.92.9b, miniz 3.1.2, LZMA SDK 25.01 (decoder, public domain), nlohmann/json 3.12, nanosvg, doctest 2.4.12. CMake downloads them into `.deps/` and checks their SHA-256. The UI fonts are in `res/fonts`: IBM Plex Sans and IBM Plex Mono (OFL), Phosphor 2.1 icons (MIT). The language flags are SVGs from flag-icons 7.5 (`res/flags`, MIT), rendered with nanosvg.

## Testing and debugging

```powershell
.\tools\make_fixtures.ps1   # test copies of game folders in out\fixtures
.\build\x86-release\uf-installer.exe --target out\fixtures\bare --no-elevate --fonts-dir out\fonts
```

`uf-installer.exe` switches for testing:

| Switch | Effect |
|---|---|
| `--target <folder>` | open the inspection screen for this folder |
| `--install` | start installing right away |
| `--no-elevate` | never ask for administrator rights |
| `--fonts-dir <folder>` | install fonts there instead of `C:\Windows\Fonts` |
| `--simulate-missing-fonts` | act as if the fonts were missing |
| `--simulate-missing-d3dx9` | act as if `d3dx9_43.dll` were missing |
| `--arizona-root <folder>`, `--arizona-settings <file>` | substitute the Arizona launcher and its `settings.json` |
| `--no-scan` | skip the drive scan |
| `--lang ru\|uk\|en`, `--theme dark\|light` | language and theme instead of the system ones |
| `--screenshot <png> [--screenshot-delay ms]` | save a screenshot of the window |
| `--throttle-ms N`, `--fail-after N` | slow the installation down / fail on purpose to test the rollback |

The log is written to `%TEMP%\uf-installer.log`.
