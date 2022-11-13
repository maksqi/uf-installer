#[macro_export]
macro_rules! main_message {
    () => {
        r"
# Welcome to UltraFuck! Installer source code https://github.com/maksqi/uf-installer

Это официальный установщик UltraFuck который поможет тебе установить все нужное для правильной работы чита.
Установка займет несколько секунд.

По умолчанию GTA в Arizona Launcher расположенна по пути:

    C:\ARIZONA GAMES\bin\Arizona

Последняя запущенная GTA на этом компьютере расположена по пути:

    {last_gta_path}

Выберите действие:

    1). Установить UltraFuck в GTA: {last_gta_path}
    2). Ввести путь к GTA вручную
    3). Выход
"};
}

#[macro_export]
macro_rules! choose_installs {
    () => {
        r"
Выберите действие:

    1). Установить (ВСЕ) UltraFuck, moonloader, sampfuncs, cleo, asi loader

    2). Установить UltraFuck
    3). Установить moonloader
    4). Установить sampfuncs
    5). Установить cleo
    6). Установить asi loader
    7). Выход
"};
}