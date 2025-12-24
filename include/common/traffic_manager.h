#pragma once

#include <atomic>
#include <mutex>

namespace clash {
namespace common {

class TrafficManager {
public:
    static TrafficManager& instance();

    void addUpload(uint64_t bytes);
    void addDownload(uint64_t bytes);

    uint64_t totalUpload() const;
    uint64_t totalDownload() const;

    // Reset counters (optional)
    void reset();

private:
    TrafficManager() = default;
    ~TrafficManager() = default;
    TrafficManager(const TrafficManager&) = delete;
    TrafficManager& operator=(const TrafficManager&) = delete;

    std::atomic<uint64_t> upload_{0};
    std::atomic<uint64_t> download_{0};
};

} // namespace common
} // namespace clash
