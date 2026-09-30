#include "core/payload.h"

#include <windows.h>

#include <LzmaDec.h>
#include <miniz.h>

#include <format>
#include <new>
#include <stdexcept>
#include <vector>

#include "core/fsutil.h"
#include "core/i18n.h"
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

namespace {

constexpr mz_uint16 kMethodLzma = 14;

void* LzmaAlloc(ISzAllocPtr, size_t size) { return ::operator new(size, std::nothrow); }
void LzmaFree(ISzAllocPtr, void* address) { ::operator delete(address); }

// Decodes a zip LZMA entry: [version:2][props size:2][props:5][raw LZMA stream, usually with an end mark].
bool DecodeLzma(const std::vector<std::uint8_t>& in, std::vector<std::uint8_t>& out) {
    if (in.size() < 4 + LZMA_PROPS_SIZE) return false;
    std::size_t propsSize = in[2] | (in[3] << 8);
    if (propsSize != LZMA_PROPS_SIZE || in.size() < 4 + propsSize) return false;
    const Byte* props = in.data() + 4;
    SizeT srcLen = in.size() - 4 - propsSize;
    SizeT destLen = out.size();
    ELzmaStatus status = LZMA_STATUS_NOT_SPECIFIED;
    ISzAlloc alloc{LzmaAlloc, LzmaFree};
    SRes res = LzmaDecode(out.data(), &destLen, props + propsSize, &srcLen, props, LZMA_PROPS_SIZE, LZMA_FINISH_END, &status, &alloc);
    return res == SZ_OK && destLen == out.size() &&
           (status == LZMA_STATUS_FINISHED_WITH_MARK || status == LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK);
}

}  // namespace

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
        error_ = T("Встроенный архив с файлами не найден — установщик повреждён.",
                   "The embedded file archive is missing: the installer is damaged.");
        return;
    }
    if (!mz_zip_reader_init_mem(&impl_->zip, bytes.data(), bytes.size(), 0)) {
        error_ = F("Встроенный архив повреждён: {}", "The embedded archive is damaged: {}", mz_zip_get_error_string(mz_zip_get_last_error(&impl_->zip)));
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
    if (index < 0) throw std::runtime_error(F("В установщике нет файла {}", "The installer has no file {}", name));

    mz_zip_archive_file_stat stat{};
    if (!mz_zip_reader_file_stat(&impl_->zip, static_cast<mz_uint>(index), &stat))
        throw std::runtime_error(F("Ошибка распаковки {}: {}", "Could not unpack {}: {}", name,
                                   mz_zip_get_error_string(mz_zip_get_last_error(&impl_->zip))));
    // LZMA entries (miniz only knows deflate): read the raw stream, decode in memory, check the CRC.
    std::vector<std::uint8_t> data;
    if (stat.m_method == kMethodLzma) {
        std::vector<std::uint8_t> packed(static_cast<std::size_t>(stat.m_comp_size));
        data.resize(static_cast<std::size_t>(stat.m_uncomp_size));
        bool ok = mz_zip_reader_extract_to_mem(&impl_->zip, static_cast<mz_uint>(index), packed.data(), packed.size(),
                                               MZ_ZIP_FLAG_COMPRESSED_DATA) &&
                  DecodeLzma(packed, data) && mz_crc32(MZ_CRC32_INIT, data.data(), data.size()) == stat.m_crc32;
        if (!ok) throw std::runtime_error(F("Ошибка распаковки {}: {}", "Could not unpack {}: {}", name,
                                                 T("данные повреждены", "the data is damaged")));
    }

    CreateDirs(dest.parent_path());
    HANDLE f = CreateFileW(ExtendedPath(dest).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) {
        DWORD e = GetLastError();
        throw FsError(e, F("Не удалось создать временный файл «{}»: {}", "Could not create the temporary file \"{}\": {}", PathUtf8(dest),
                           Win32ErrorText(e)));
    }
    auto write = [](void* opaque, mz_uint64, const void* buf, size_t n) -> size_t {
        DWORD written = 0;
        if (!WriteFile(static_cast<HANDLE>(opaque), buf, static_cast<DWORD>(n), &written, nullptr)) return 0;
        return written;
    };
    mz_bool ok = stat.m_method == kMethodLzma
                     ? write(f, 0, data.data(), data.size()) == data.size()
                     : mz_zip_reader_extract_to_callback(&impl_->zip, static_cast<mz_uint>(index), write, f, 0);
    CloseHandle(f);
    if (!ok) {
        DeleteFileW(ExtendedPath(dest).c_str());
        throw std::runtime_error(F("Ошибка распаковки {}: {}", "Could not unpack {}: {}", name,
                                             mz_zip_get_error_string(mz_zip_get_last_error(&impl_->zip))));
    }
}

}  // namespace uf
