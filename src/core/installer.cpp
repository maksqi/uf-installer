#include "core/installer.h"

#include <windows.h>

#include <chrono>
#include <format>
#include <random>

#include "core/directx.h"
#include "core/fonts.h"
#include "core/fsutil.h"
#include "core/log.h"
#include "core/payload.h"

namespace uf {

namespace {

class InstallError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct JournalEntry {
    enum Kind { Noop, Created, Replaced, Moved, TempFile } kind = Noop;
    fs::path path;
    fs::path backup;
};

std::string Timestamp() {
    SYSTEMTIME t;
    GetLocalTime(&t);
    return std::format("{:04}-{:02}-{:02}_{:02}-{:02}-{:02}", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
}

fs::path EntryPath(std::string_view entry) {
    std::wstring w = ToWide(entry);
    for (wchar_t& c : w)
        if (c == L'/') c = L'\\';
    return fs::path(w);
}

void DeleteIfExists(const fs::path& p) {
    ClearReadOnly(p);
    if (!DeleteFileW(ExtendedPath(p).c_str())) {
        DWORD e = GetLastError();
        if (e != ERROR_FILE_NOT_FOUND && e != ERROR_PATH_NOT_FOUND)
            throw FsError(e, std::format("Не удалось удалить «{}»: {}", PathUtf8(p), Win32ErrorText(e)));
    }
}

// Removes empty folders bottom-up; stops at folders that still contain files.
bool RemoveEmptyTree(const fs::path& dir) {
    if (!DirExists(dir)) return true;
    bool empty = true;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir / L"*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            std::wstring name = fd.cFileName;
            if (name == L"." || name == L"..") continue;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                empty &= RemoveEmptyTree(dir / name);
            else
                empty = false;
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    return empty && RemoveDirectoryW(ExtendedPath(dir).c_str());
}

std::string Rollback(const std::vector<JournalEntry>& journal, const std::vector<fs::path>& createdDirs,
                     const fs::path& backupRoot) {
    std::string errors;
    for (auto it = journal.rbegin(); it != journal.rend(); ++it) {
        try {
            switch (it->kind) {
                case JournalEntry::Created:
                case JournalEntry::TempFile: DeleteIfExists(it->path); break;
                case JournalEntry::Replaced: MoveFileRetry(it->backup, it->path, true); break;
                case JournalEntry::Moved: MoveFileRetry(it->backup, it->path, false); break;
                case JournalEntry::Noop: break;
            }
        } catch (const std::exception& e) {
            errors += e.what();
            errors += '\n';
        }
    }
    for (auto d = createdDirs.rbegin(); d != createdDirs.rend(); ++d) RemoveDirectoryW(ExtendedPath(*d).c_str());
    RemoveEmptyTree(backupRoot);
    RemoveDirectoryW(ExtendedPath(backupRoot.parent_path()).c_str());  // uf-installer-backup, only if empty
    return errors;
}

void WriteTextFile(const fs::path& path, const std::string& text) {
    HANDLE f = CreateFileW(ExtendedPath(path).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return;
    static const char bom[] = "\xEF\xBB\xBF";
    DWORD written = 0;
    WriteFile(f, bom, 3, &written, nullptr);
    WriteFile(f, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
    CloseHandle(f);
}

}  // namespace

TempDir::TempDir() {
    std::random_device rd;
    fs::path base = GetFolder(Folder::Temp);
    for (int i = 0; i < 16; ++i) {
        fs::path p = base / std::format(L"uf-installer-{}-{:08x}", GetCurrentProcessId(), rd());
        if (CreateDirectoryW(p.c_str(), nullptr)) {
            path_ = p;
            return;
        }
    }
    throw InstallError(std::format("Не удалось создать временную папку в {}", PathUtf8(base)));
}

TempDir::~TempDir() {
    if (path_.empty()) return;
    std::error_code ec;
    fs::remove_all(path_, ec);
}

void CleanupStaleTempDirs() {
    fs::path base = GetFolder(Folder::Temp);
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((base / L"uf-installer-*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    ULARGE_INTEGER n{{now.dwLowDateTime, now.dwHighDateTime}};
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        ULARGE_INTEGER t{{fd.ftLastWriteTime.dwLowDateTime, fd.ftLastWriteTime.dwHighDateTime}};
        if (n.QuadPart > t.QuadPart && n.QuadPart - t.QuadPart > 24ull * 3600 * 10'000'000) {
            std::error_code ec;
            fs::remove_all(base / fd.cFileName, ec);
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

InstallResult RunInstall(const fs::path& gameDir, const InstallPlan& plan, const InstallEnv& env, const ProgressFn& progress,
                         const LogFn& logLine, std::stop_token stop) {
    InstallResult res;
    const auto started = std::chrono::steady_clock::now();
    auto elapsed = [&] { return std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count(); };
    std::string transcript;
    auto say = [&](const std::string& s) {
        log::Info("{}", s);
        transcript += s + "\r\n";
        if (logLine) logLine(s);
    };

    std::size_t copyOps = 0;
    for (const FileOp& op : plan.files) copyOps += op.kind == OpKind::Copy;
    const std::size_t totalSteps = std::max<std::size_t>(1, copyOps + plan.files.size() + plan.fonts.size() * 2);
    const float fileShare = plan.directx ? 0.8f : 1.0f;
    std::size_t step = 0;
    auto report = [&](const std::string& stage, const std::string& current) {
        if (progress) progress({fileShare * static_cast<float>(step) / static_cast<float>(totalSteps), stage, current});
    };
    auto tick = [&](const std::string& stage, const std::string& current) {
        ++step;
        report(stage, current);
    };

    std::vector<JournalEntry> journal;
    std::vector<fs::path> createdDirs;
    const fs::path backupRoot = gameDir / L"uf-installer-backup" / ToWide(Timestamp());
    std::optional<TempDir> temp;
    auto staged = [&](std::string_view entry) { return temp->path() / L"stage" / EntryPath(entry); };

    say(std::format("Установка в {}", PathUtf8(gameDir)));
    try {
        // ---- Preflight
        report("Подготовка", "");
        bool running = false, maybe = false;
        DetectRunningGame(gameDir, &running, &maybe);
        if (running) throw InstallError("Игра запущена из этой папки — закройте её и повторите установку.");
        if (!HasGtaExe(gameDir)) throw InstallError("В папке нет gta_sa.exe.");
        const std::uint64_t need = plan.BytesToWrite();
        if (auto free = FreeSpace(gameDir); free && *free < need + (16u << 20))
            throw InstallError(std::format("Недостаточно места на диске: нужно ещё {:.1f} МБ.", (need + (16u << 20) - *free) / 1048576.0));
        Payload& payload = Payload::Instance();
        if (!payload.ok()) throw InstallError(payload.error());
        temp.emplace();
        say(std::format("Распаковка во временную папку {}", PathUtf8(temp->path())));

        // ---- Stage: extract everything we need before touching the game folder
        for (const FileOp& op : plan.files) {
            if (op.kind != OpKind::Copy) continue;
            if (stop.stop_requested()) throw InstallError("Установка отменена.");
            fs::path out = staged(op.entry);
            payload.Extract(op.entry, out);
            if (FileSize(out).value_or(0) != op.size) throw InstallError(std::format("Файл {} распакован с ошибкой.", op.entry));
            tick("Распаковка", PathUtf8(op.rel));
        }
        for (const FontOp& f : plan.fonts) {
            payload.Extract(f.entry, staged(f.entry));
            tick("Распаковка", f.file);
        }

        // ---- Commit
        if (!plan.files.empty() || !plan.dirs.empty()) say("Копирование файлов в папку игры…");
        for (const fs::path& rel : plan.dirs) {
            auto created = CreateDirs(gameDir / rel);
            res.dirsCreated += static_cast<int>(created.size());
            createdDirs.insert(createdDirs.end(), created.begin(), created.end());
        }
        int committed = 0;
        for (const FileOp& op : plan.files) {
            if (stop.stop_requested()) throw InstallError("Установка отменена.");
            if (env.failAfter >= 0 && committed >= env.failAfter)
                throw InstallError(std::format("Тестовый сбой после {} операций (--fail-after).", committed));
            const fs::path dest = gameDir / op.rel;
            const std::string rel = PathUtf8(op.rel);

            if (op.kind == OpKind::MoveToBackup) {
                if (FileExists(dest)) {
                    fs::path bak = backupRoot / op.rel;
                    CreateDirs(bak.parent_path());
                    ClearReadOnly(dest);
                    MoveFileRetry(dest, bak, false);
                    journal.push_back({JournalEntry::Moved, dest, bak});
                    ++res.movedOld;
                    say(std::format("Старая версия перенесена в резервную копию: {}", rel));
                }
            } else {
                const bool exists = FileExists(dest);
                if (exists && (op.mode == CopyMode::AddIfMissing || FileMatches(dest, op.size, op.crc))) {
                    ++res.skipped;
                } else {
                    auto created = CreateDirs(dest.parent_path());
                    createdDirs.insert(createdDirs.end(), created.begin(), created.end());
                    fs::path tmp = dest;
                    tmp += L".uf-tmp";
                    CopyFileRetry(staged(op.entry), tmp);
                    const std::size_t tmpIndex = journal.size();
                    journal.push_back({JournalEntry::TempFile, tmp, {}});
                    if (exists) {
                        fs::path bak = backupRoot / op.rel;
                        CreateDirs(bak.parent_path());
                        ClearReadOnly(dest);
                        MoveFileRetry(dest, bak, false);
                        journal.push_back({JournalEntry::Replaced, dest, bak});
                    }
                    MoveFileRetry(tmp, dest, true);
                    journal[tmpIndex] = exists ? JournalEntry{} : JournalEntry{JournalEntry::Created, dest, {}};
                    if (exists) {
                        ++res.replaced;
                        say(std::format("Заменён (старый — в резервной копии): {}", rel));
                    } else {
                        ++res.installed;
                    }
                }
            }
            ++committed;
            tick("Установка файлов", rel);
            if (env.throttleMs > 0) Sleep(static_cast<DWORD>(env.throttleMs));
        }

        // ---- Verify
        for (const FileOp& op : plan.files)
            if (op.kind == OpKind::Copy && !FileExists(gameDir / op.rel))
                throw InstallError(std::format("После установки не найден файл {} — возможно, его удалил антивирус.", PathUtf8(op.rel)));
        for (const fs::path& rel : plan.dirs)
            if (!DirExists(gameDir / rel)) throw InstallError(std::format("Не удалось создать папку {}", PathUtf8(rel)));
        say(std::format("Файлы игры: новых {}, заменено {}, уже были {}, старых версий убрано {}.", res.installed, res.replaced,
                        res.skipped, res.movedOld));
    } catch (const std::exception& e) {
        res.error = e.what();
        log::Error("Install failed: {}", res.error);
        say("Ошибка: " + res.error);
        if (!journal.empty() || !createdDirs.empty()) {
            say("Откат изменений…");
            std::string rollbackErrors = Rollback(journal, createdDirs, backupRoot);
            res.rolledBack = true;
            if (!rollbackErrors.empty()) res.error += "\nОткат выполнен не полностью:\n" + rollbackErrors;
            else say("Все изменения отменены, папка игры в исходном состоянии.");
        }
        if (auto fe = dynamic_cast<const FsError*>(&e); fe && fe->accessDenied())
            res.error += "\n\nНет доступа к файлу. Запустите установщик от имени администратора или проверьте, не блокирует ли запись "
                         "антивирус (в Защитнике Windows — «Контролируемый доступ к папкам»).";
        res.seconds = elapsed();
        return res;
    }

    // ---- System-wide steps. Their failure does not undo the game files.
    if (!plan.fonts.empty()) {
        for (const FontOp& f : plan.fonts) {
            try {
                InstallFontFile(staged(f.entry), env.fontsDir, f.file, f.regName, env.registerFonts);
                ++res.fontsInstalled;
                say(std::format("Шрифт установлен: {} ({})", f.file, f.regName));
            } catch (const std::exception& e) {
                res.warnings.push_back(std::format("Шрифт {} не установлен: {}", f.file, e.what()));
                log::Warn("{}", res.warnings.back());
            }
            tick("Установка шрифтов", f.file);
        }
        if (res.fontsInstalled && env.registerFonts) BroadcastFontChange();
    }
    if (plan.directx) {
        if (progress) progress({0.85f, "DirectX", "Скачивание…"});
        DxResult dx = InstallDirectX(temp->path() / L"dx", [&](const std::string& s) {
            say(s);
            if (progress) progress({0.9f, "DirectX", s});
        }, stop);
        if (dx.ok && HasD3DX9_43(gameDir)) {
            res.directxInstalled = true;
            say(dx.message);
        } else {
            res.warnings.push_back(dx.ok ? "Установщик DirectX завершился, но d3dx9_43.dll не появился — перезагрузите ПК и запустите установщик снова."
                                         : dx.message);
        }
    }

    for (const std::string& w : res.warnings) say("Предупреждение: " + w);
    if (DirExists(backupRoot)) {
        res.backupDir = backupRoot;
        WriteTextFile(backupRoot / L"install.log", transcript);
    }
    res.ok = true;
    res.seconds = elapsed();
    if (progress) progress({1.0f, "Готово", ""});
    say(std::format("Готово за {:.1f} с.", res.seconds));
    return res;
}

}  // namespace uf
