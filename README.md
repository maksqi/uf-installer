# UltraFuck Setup

Установщик MoonLoader-скрипта **UltraFuck** для GTA San Andreas (SA-MP). Это один `.exe` около 12 МБ, все нужные файлы вшиты внутрь.

Что делает установщик:

- **Ищет игру.** Находит все папки с `gta_sa.exe`. Сначала за ~0,1 с проверяет реестр SA-MP, **Arizona Games Launcher** (`settings.json` → `gamePath` или `<лаунчер>\bin\<id>`), историю запусков, ярлыки и установленные программы. Затем за несколько секунд параллельно сканирует все локальные диски. Папку можно указать и вручную.
- **Анализирует выбранную папку.** Определяет:
  - версию `gta_sa.exe`;
  - сборку SA-MP (по entry point `samp.dll`);
  - ASI-загрузчик: Silent, Ultimate или загрузчик Arizona;
  - CLEO, SAMPFUNCS, MoonLoader и их версии;
  - библиотеки MoonLoader;
  - установленные версии UltraFuck;
  - наличие `d3dx9_43.dll` и шрифтов Trebuchet MS (`imgui.lua` без `trebucbd.ttf` падает).
- **Ставит только то, чего не хватает.** Архивы сначала распаковываются в `%TEMP%\uf-installer-*`. Каждый заменяемый файл сохраняется в `<игра>\uf-installer-backup\<дата>\`. При любой ошибке все изменения откатываются.
- **Не трогает то, чем управляет Arizona Launcher**: загрузчик, CLEO, SAMPFUNCS, `bass.dll`.
- **Бережёт настройки и старые версии.** Ставит UltraFuck той версии, что вшита в exe; другие версии (например `UltraFuck 2.4.lua`) переносит в резервную копию. `Settings.ini` с логином не перезаписывается.
- **Просит права администратора только когда нужно**: шрифты в `C:\Windows\Fonts`, DirectX, папка игры без права записи. В этом случае установщик перезапускает себя через UAC.

## Сборка

Нужны Visual Studio 2022/2026 Build Tools (C++ x86, toolset **14.44**), Python 3 и WinRAR (`UnRAR.exe` — для архива со шрифтами).

```powershell
.\build.ps1                 # Release: сборка + тесты + проверка импортов (Windows 7+)
.\build.ps1 -Clean          # с нуля
.\build.ps1 -Payload D:\uf  # архивы лежат в другой папке
```

Результат: `build\x86-release\uf-installer.exe`.

Архивы с файлами в git не хранятся. По умолчанию они берутся из `%USERPROFILE%\Desktop\uf-installer`:

| Архив | Что внутри |
|---|---|
| `silents_asi_loader_13.zip` | ASI Loader |
| `CLEO.zip` | CLEO 4.3.22 |
| `SAMPFUNCS_5.4.1.zip` | SAMPFUNCS (только SA-MP R1) |
| `moonloader_0.26.zip` | MoonLoader 026.5 |
| `UltraFuck_<версия>.zip` | скрипт, `config/`, `lib/` |
| `trebucbi.rar` | шрифты Trebuchet MS |

**Обновить скрипт:** положите новый `UltraFuck_<версия>.zip` вместо старого и запустите `.\build.ps1`. Версия берётся из имени архива. Упаковщик `tools/pack_payload.py` сам раскладывает файлы по компонентам и разрешает конфликты между архивами. Его отчёт лежит в `build\x86-release\generated\payload_report.txt`.

## Устройство

```
src/core/   анализ папки, план, установка, поиск игр, Arizona, шрифты, DirectX, UAC (без UI)
src/ui/     окно Win32 + Direct3D 9Ex + Dear ImGui 1.92 (тёмная тема, свой заголовок, Per-Monitor DPI)
src/cli/    uf-cli.exe — то же ядро из консоли (для тестов)
tests/      doctest: юнит-тесты + установка/откат в настоящую временную папку
tools/      упаковщик payload, генератор иконки, фикстуры, аудит импортов
```

Библиотеки: Dear ImGui 1.92.9b, miniz 3.1.2, nlohmann/json 3.12, doctest 2.4.12. Все они скачиваются CMake с проверкой SHA-256 в `.deps/`. Шрифты интерфейса: Inter и Font Awesome 6 Free (`res/fonts`, лицензия OFL).

## Проверка и отладка

```powershell
.\build\x86-release\uf-cli.exe scan                       # найти все игры (+ время скана)
.\build\x86-release\uf-cli.exe plan "D:\GTA" [--json]     # что будет сделано (ничего не меняет)
.\build\x86-release\uf-cli.exe install "D:\GTA" --yes     # установить из консоли
.\build\x86-release\uf-cli.exe dxcheck                    # скачать dxwebsetup и проверить подпись Microsoft
.\tools\make_fixtures.ps1                                 # тестовые копии папок игры в out\fixtures
```

Ключи `uf-installer.exe` и `uf-cli.exe` для тестов:

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
| `--screenshot <png> [--screenshot-delay мс]` | снимок окна (GUI) |
| `--throttle-ms N`, `--fail-after N` | замедлить установку / искусственный сбой для проверки отката |

Лог работы пишется в `%TEMP%\uf-installer.log`.
