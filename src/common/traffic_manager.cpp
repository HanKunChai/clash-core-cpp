#include "common/traffic_manager.h"

namespace clash
{
namespace common
{

TrafficManager& TrafficManager::instance()
{
    static TrafficManager instance;
    return instance;
}

// 增加上传流量计数
void TrafficManager::addUpload(uint64_t bytes)
{
    upload_.fetch_add(bytes, std::memory_order_relaxed);
}

// 增加下载流量计数
void TrafficManager::addDownload(uint64_t bytes)
{
    download_.fetch_add(bytes, std::memory_order_relaxed);
}

// 获取总上传流量
uint64_t TrafficManager::totalUpload() const
{
    return upload_.load(std::memory_order_relaxed);
}

// 获取总下载流量
uint64_t TrafficManager::totalDownload() const
{
    return download_.load(std::memory_order_relaxed);
}

// 重置流量计数
void TrafficManager::reset()
{
    upload_.store(0, std::memory_order_relaxed);
    download_.store(0, std::memory_order_relaxed);
}

} // namespace common
} // namespace clash
