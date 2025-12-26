#include "common/connection_manager.h"

namespace clash
{
namespace common
{

// 获取 ConnectionManager 单例
ConnectionManager& ConnectionManager::instance()
{
    static ConnectionManager instance;
    return instance;
}

// 添加连接到管理器
// 使用 weak_ptr 存储，避免循环引用
void ConnectionManager::add(std::shared_ptr<Trackable> conn)
{
    std::lock_guard<std::mutex> lock(mutex_);
    connections_[conn->id()] = conn;
}

// 从管理器中移除连接
void ConnectionManager::remove(const std::string& id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    connections_.erase(id);
}

// 获取所有活跃连接的快照
// 过滤掉已经失效的 weak_ptr
std::vector<std::shared_ptr<Trackable>> ConnectionManager::getAll() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<Trackable>> result;
    
    for (const auto& [id, weak_conn] : connections_)
    {
        if (auto conn = weak_conn.lock())
        {
            result.push_back(conn);
        }
    }
    return result;
}

// 关闭指定 ID 的连接
void ConnectionManager::close(const std::string& id)
{
    std::shared_ptr<Trackable> target;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = connections_.find(id);
        if (it != connections_.end())
        {
            target = it->second.lock();
        }
    }
    
    if (target)
    {
        target->close();
    }
}

} // namespace common
} // namespace clash
