#pragma once

#include "common/trackable.h"
#include <memory>
#include <mutex>
#include <map>
#include <vector>

namespace clash {
namespace common {

class ConnectionManager {
public:
    static ConnectionManager& instance();

    void add(std::shared_ptr<Trackable> conn);
    void remove(const std::string& id);
    
    std::vector<std::shared_ptr<Trackable>> getAll() const;
    void close(const std::string& id);

private:
    ConnectionManager() = default;
    
    mutable std::mutex mutex_;
    std::map<std::string, std::weak_ptr<Trackable>> connections_;
};

} // namespace common
} // namespace clash
