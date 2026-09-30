# UltraFu#k Setup

[English](README.md) · **Русский**

Установщик MoonLoader-скрипта **UltraFu#k** для GTA San Andreas (SA-MP). Это один `.exe` около 11 МБ, все нужные файлы вшиты внутрь.

**[Скачать uf-installer.exe](https://github.com/maksqi/uf-installer/releases/latest/download/uf-installer.exe)** (последняя версия, все версии — в [Releases](https://github.com/maksqi/uf-installer/releases)). Работает на Windows 7–11, ничего дополнительно ставить не нужно.

Что делает установщик:

- **Ищет игру.** Находит все папки с `gta_sa.exe`. Сначала за ~0,1 с проверяет реестр SA-MP, **Arizona Games Launcher** (`settings.json` → `gamePath` или `<лаунчер>\bin\<id>`), историю запусков, ярлыки и установленные программы. Затем за несколько секунд параллельно сканирует все локальные диски. Папку можно указать и вручную.
- **Анализирует выбранную папку.** Определяет:
  - версию `gta_sa.exe`;
  - сборку SA-MP (по entry point `samp.dll`);
  - ASI-загрузчик: Silent, Ultimate или загрузчик Arizona;
  - CLEO, SAMPFUNCS, MoonLoader и их версии;
  - библиотеки MoonLoader;
  - установленные версии UltraFu#k;
  - наличие `d3dx9_43.dll` и шрифтов Trebuchet MS (`imgui.lua` без `trebucbd.ttf` падает).
- **Ставит только то, чего не хватает.** Архивы сначала распаковываются в `%TEMP%\uf-installer-*`. Каждый заменяемый файл сохраняется в `<игра>\uf-installer-backup\<дата>\`. При любой ошибке все изменения откатываются.
- **Не трогает то, чем управляет Arizona Launcher**: загрузчик, CLEO, SAMPFUNCS, `bass.dll`.
- **Бережёт настройки и старые версии.** Ставит UltraFu#k той версии, что вшита в exe; другие версии (например `UltraFu#k 2.4.lua`) переносит в резервную копию. `Settings.ini` с логином не перезаписывается.
- **Просит права администратора только когда нужно**: шрифты в `C:\Windows\Fonts`, DirectX, папка игры без права записи. В этом случае установщик перезапускает себя через UAC.
- **Обновляет библиотеки MoonLoader.** Галочка «Перезаписать все библиотеки» включена по умолчанию: библиотеки, которые отличаются от комплекта, заменяются (старые уходят в резервную копию), одинаковые не трогаются.
- **Светлая и тёмная тема; русский, украинский и английский.** Тема берётся из настроек Windows и меняется вместе с ними. Язык: украинский для украинской Windows, русский для русской и других языков СНГ, иначе английский. Тему и язык можно сменить в заголовке окна (язык — в меню с флагами).

## Сборка

Нужны Visual Studio 2022/2026 Build Tools (C++ x86, toolset **14.44**) и Python 3.

```powershell
.\build.ps1                 # Release: сборка + тесты + проверка импортов (Windows 7+)
.\build.ps1 -Clean          # с нуля
.\build.ps1 -Payload D:\uf  # архивы лежат в другой папке
```

Результат: `build\x86-release\uf-installer.exe`.

Устанавливаемые файлы лежат в папке `payload/`:

| Архив | Что внутри |
|---|---|
| `silents_asi_loader_13.zip` | ASI Loader |
| `CLEO.zip` | CLEO 4.3.22 |
| `SAMPFUNCS_5.4.1.zip` | SAMPFUNCS (только SA-MP R1) |
| `moonloader_0.26.zip` | MoonLoader 026.5 |
| `UltraFu#k_2.36.zip` | скрипт, `config/`, `lib/` |
| `trebuchet_ms.zip` | шрифты Trebuchet MS |

Архивы сжаты LZMA с максимальными настройками (preset 9e). Внутри exe файлы тоже лежат одним LZMA-архивом. Такие zip открывают WinRAR и 7-Zip, а Проводник Windows — нет.

**Обновить скрипт:** положите новый архив скрипта `<имя>_<версия>.zip` в `payload/` вместо старого и запустите `.\build.ps1`. Версия берётся из имени архива. Упаковщик `tools/pack_payload.py` сам определяет, что лежит в каждом архиве, раскладывает файлы по компонентам и разрешает конфликты между архивами. Его отчёт лежит в `build\x86-release\generated\payload_report.txt`.

**Пережать архив:** `python tools/repack_zip.py payload\<архив>.zip`. Папку можно упаковать так: `python tools/repack_zip.py <папка> -o payload\<архив>.zip`. Упаковщик принимает только `.zip`, поэтому `.rar` и `.7z` сначала распакуйте и упакуйте этой командой.

**Тексты интерфейса** написаны в коде парами `T("русский", "english")`. Украинский перевод лежит в `src/core/i18n_uk.cpp`, ключ — русский текст. `tools/i18n_check.py` запускается при сборке и падает, если какой-то строке не хватает перевода.

**Сменить иконку:** запустите `.\tools\make_icon.ps1 -Source <картинка.png>` (квадратная, лучше от 256 px). Скрипт пересоберёт `res/app.ico`: размеры 16–256 px, скруглённые углы. Эта же иконка показывается в заголовке окна.

## Релизы

GitHub Actions (`.github/workflows/build.yml`) собирает exe на каждый push и pull request. В сборку входят тесты, проверка перевода и проверка импортов, а exe сохраняется в артефактах запуска.

Чтобы выпустить версию, поднимите `VERSION` в `CMakeLists.txt`, закоммитьте и отправьте тег:

```powershell
git tag v1.0.1
git push origin v1.0.1
```

Workflow проверит, что тег совпадает с версией, соберёт exe и опубликует его в Releases. Ссылка «Скачать» выше всегда ведёт на последний релиз.

## Устройство

```
src/core/   анализ папки, план, установка, поиск игр, Arizona, шрифты, DirectX, UAC (без UI)
src/ui/     окно Win32 + Direct3D 9Ex + Dear ImGui 1.92 (чёрно-белая тема, свой заголовок, Per-Monitor DPI)
tests/      doctest: юнит-тесты + установка/откат в настоящую временную папку
tools/      упаковщик payload, пережатие архивов, генератор иконки, фикстуры, аудит импортов
payload/    архивы с устанавливаемыми файлами
```

Библиотеки: Dear ImGui 1.92.9b, miniz 3.1.2, LZMA SDK 25.01 (декодер, public domain), nlohmann/json 3.12, nanosvg, doctest 2.4.12. Все они скачиваются CMake с проверкой SHA-256 в `.deps/`. Шрифты интерфейса лежат в `res/fonts`: IBM Plex Sans и IBM Plex Mono (лицензия OFL), иконки Phosphor 2.1 (лицензия MIT). Флаги языков — SVG из flag-icons 7.5 (`res/flags`, лицензия MIT), их рисует nanosvg.

## Проверка и отладка

```powershell
.\tools\make_fixtures.ps1   # тестовые копии папок игры в out\fixtures
.\build\x86-release\uf-installer.exe --target out\fixtures\bare --no-elevate --fonts-dir out\fonts
```

Ключи `uf-installer.exe` для тестов:

| Ключ | Что делает |
|---|---|
| `--target <папка>` | открыть сразу экран анализа |
| `--install` | сразу начать установку |
| `--no-elevate` | не запрашивать права администратора |
| `--fonts-dir <папка>` | ставить шрифты в эту папку вместо `C:\Windows\Fonts` |
| `--simulate-missing-fonts` | считать, что шрифтов нет |
| `--simulate-missing-d3dx9` | считать, что `d3dx9_43.dll` нет |
| `--arizona-root <папка>`, `--arizona-settings <файл>` | подменить лаунчер Arizona и его `settings.json` |
| `--no-scan` | не сканировать диски |
| `--lang ru\|uk\|en`, `--theme dark\|light` | язык и тема вместо системных |
| `--screenshot <png> [--screenshot-delay мс]` | снимок окна |
| `--throttle-ms N`, `--fail-after N` | замедлить установку / искусственный сбой для проверки отката |

Лог работы пишется в `%TEMP%\uf-installer.log`.
