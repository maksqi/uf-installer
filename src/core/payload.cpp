#include "core/payload.h"

#include <windows.h>

#include <miniz.h>

#include <format>
#include <stdexcept>

#include "core/fsutil.h"
#include "resource.h"

namespace uf {

std::span<const std::uint8_t> ResourceBytes(int id) {
    HMODULE module = GetModuleHandleW(nullptr);
    HRSRC res = FindResourceW(module, MAKEINTRESOURCEW(id), RT_RCDATA);
    if (!res) return {};
    HGLOBAL handle = LoadResource(module, res);
    if (!handle) return {};
    const void* data = LockResource(handle);
    DWORD size = SizeofResource(module, res);
    if (!data || size == 0) return {};
    return {static_cast<const std::uint8_t*>(data), size};
}

struct Payload::Impl {
    mz_zip_archive zip{};
};

Payload& Payload::Instance() {
    static Payload instance;
    return instance;
}

Payload::Payload() : impl_(std::make_unique<Impl>()) {
    auto bytes = ResourceBytes(IDR_PAYLOAD);
    if (bytes.empty()) {
        error_ = "Встроенный архив с файлами не найден — установщик повреждён.";
        return;
    }
    if (!mz_zip_reader_init_mem(&impl_->zip, bytes.data(), bytes.size(), 0)) {
        error_ = std::format("Встроенный архив повреждён: {}", mz_zip_get_error_string(mz_zip_get_last_error(&impl_->zip)));
        return;
    }
    ok_ = true;
}

Payload::~Payload() {
    if (ok_) mz_zip_reader_end(&impl_->zip);
}

void Payload::Extract(std::string_view entry, const fs::path& dest) {
    std::lock_guard lock(mutex_);
    if (!ok_) throw std::runtime_error(error_);
    std::string name(entry);
    int index = mz_zip_reader_locate_file(&impl_->zip, name.c_str(), nullptr, 0);
    if (index < 0) throw std::runtime_error(std::format("В установщике нет файла {}", name));

    CreateDirs(dest.parent_path());
    HANDLE f = CreateFileW(ExtendedPath(dest).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) {
        DWORD e = GetLastError();
        throw FsError(e, std::format("Не удалось создать временный файл «{}»: {}", PathUtf8(dest), Win32ErrorText(e)));
    }
    auto write = [](void* opaque, mz_uint64, const void* buf, size_t n) -> size_t {
        DWORD written = 0;
        if (!WriteFile(static_cast<HANDLE>(opaque), buf, static_cast<DWORD>(n), &written, nullptr)) return 0;
        return written;
    };
    mz_bool ok = mz_zip_reader_extract_to_callback(&impl_->zip, static_cast<mz_uint>(index), write, f, 0);
    CloseHandle(f);
    if (!ok) {
        DeleteFileW(ExtendedPath(dest).c_str());
        throw std::runtime_error(std::format("Ошибка распаковки {}: {}", name,
                                             mz_zip_get_error_string(mz_zip_get_last_error(&impl_->zip))));
    }
}

}  // namespace uf
