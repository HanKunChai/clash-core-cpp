#include "common/connection_manager.h"

namespace clash {
namespace common {

ConnectionManager& ConnectionManager::instance() {
    static ConnectionManager instance;
    return instance;
}

void ConnectionManager::add(std::shared_ptr<Trackable> conn) {
    std::lock_guard<std::mutex> lock(mutex_);
    connections_[conn->id()] = conn;
}

void ConnectionManager::remove(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    connections_.erase(id);
}

std::vector<std::shared_ptr<Trackable>> ConnectionManager::getAll() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<Trackable>> result;
    
    for (const auto& [id, weak_conn] : connections_) {
        if (auto conn = weak_conn.lock()) {
            result.push_back(conn);
        }
    }
    return result;
}

void ConnectionManager::close(const std::string& id) {
    std::shared_ptr<Trackable> target;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = connections_.find(id);
        if (it != connections_.end()) {
            target = it->second.lock();
        }
    }
    
    if (target) {
        target->close();
    }
}

} // namespace common
} // namespace clash
