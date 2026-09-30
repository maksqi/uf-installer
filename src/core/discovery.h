#pragma once
// Finds GTA SA folders: registry/launcher hints first (instant), then a background scan of fixed drives.
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "core/arizona.h"
#include "core/inspect.h"

namespace uf {

enum Source : std::uint32_t {
    kSrcSampRegistry = 1u << 0,
    kSrcArizona = 1u << 1,
    kSrcAppCompat = 1u << 2,
    kSrcMuiCache = 1u << 3,
    kSrcUninstall = 1u << 4,
    kSrcShortcut = 1u << 5,
    kSrcDiskScan = 1u << 6,
    kSrcManual = 1u << 7,
};

struct GameEntry {
    fs::path dir;
    std::uint32_t sources = 0;
    std::string arizonaId;
    std::string title;
    std::shared_ptr<const FolderReport> report;  // null until inspected
};

struct DiscoveryConfig {
    InspectOptions inspect;  // `arizona` is filled by Discovery itself
    bool diskScan = true;
    std::optional<fs::path> arizonaRoot;
    std::optional<fs::path> arizonaSettings;
};

class Discovery {
public:
    explicit Discovery(DiscoveryConfig config);
    ~Discovery();
    Discovery(const Discovery&) = delete;
    Discovery& operator=(const Discovery&) = delete;

    // `onChange` is called from worker threads whenever the list or a report changes.
    void Start(std::function<void()> onChange);
    void StopDiskScan();
    // Adds a user-picked folder (or the game folder found up to 2 levels below it). Returns the game folder.
    std::optional<fs::path> AddManual(const fs::path& dir);
    // Re-inspects a folder (e.g. after installation).
    void Refresh(const fs::path& dir);

    std::vector<GameEntry> Snapshot() const;
    bool HintsDone() const { return hintsDone_; }
    bool ScanRunning() const { return scanThreadsLeft_ > 0; }
    std::uint64_t DirsScanned() const { return dirsScanned_; }
    double ScanSeconds() const;
    const ArizonaInfo& Arizona() const { return arizona_; }

private:
    void Offer(const fs::path& dir, std::uint32_t source, const std::string& arizonaId = {});
    void CollectHints();
    void ScanDrive(std::wstring root, std::stop_token stop);
    void ScanDir(const std::wstring& dir, int depth, std::stop_token& stop);
    void InspectLoop(std::stop_token stop);
    void Notify();

    DiscoveryConfig config_;
    ArizonaInfo arizona_;
    std::function<void()> onChange_;

    mutable std::mutex mutex_;
    std::vector<GameEntry> entries_;
    std::unordered_map<std::wstring, std::size_t> index_;
    std::deque<std::size_t> inspectQueue_;
    std::condition_variable_any inspectCv_;

    std::atomic<bool> hintsDone_{false};
    std::atomic<int> scanThreadsLeft_{0};
    std::atomic<std::uint64_t> dirsScanned_{0};
    std::atomic<std::int64_t> scanStartMs_{0}, scanEndMs_{0};
    std::stop_source scanStop_;
    std::vector<std::jthread> threads_;
};

// gta_sa.exe in `dir` or up to `depth` levels below it.
std::optional<fs::path> ResolveGameDir(const fs::path& dir, int depth = 2);
std::string DescribeSources(std::uint32_t sources);

}  // namespace uf
