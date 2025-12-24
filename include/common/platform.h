#pragma once

#include <string>
#include <optional>

namespace clash {
namespace common {

class Platform {
public:
    // 根据源端口获取进程路径
    static std::string getProcessPath(int port);
};

} // namespace common
} // namespace clash
