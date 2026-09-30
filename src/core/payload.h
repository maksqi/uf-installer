#pragma once
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>

#include "core/paths.h"

namespace uf {

// Raw bytes of an RCDATA resource of the current exe (empty span if missing).
std::span<const std::uint8_t> ResourceBytes(int id);

// payload.zip embedded as RCDATA; entries are streamed straight to files.
class Payload {
public:
    static Payload& Instance();

    bool ok() const { return ok_; }
    const std::string& error() const { return error_; }

    // Extracts `entry` to `dest` (parent folders are created). miniz verifies the CRC.
    // Throws std::runtime_error with a readable message on failure.
    void Extract(std::string_view entry, const fs::path& dest);

    ~Payload();

private:
    Payload();
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::mutex mutex_;
    bool ok_ = false;
    std::string error_;
};

}  // namespace uf
