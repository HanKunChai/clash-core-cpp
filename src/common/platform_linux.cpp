#include "common/platform.h"
#include "log/log.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <vector>
#include <map>
#include <iostream>
#include <unistd.h>
#include <dirent.h>

namespace clash {
namespace common {

// 辅助函数：读取 /proc/net/tcp 或 tcp6 获取 inode
// 返回 map<port, inode>
std::map<int, unsigned long> getTcpPortInodeMap(const std::string& path) {
    std::map<int, unsigned long> map;
    std::ifstream f(path);
    if (!f.is_open()) return map;

    std::string line;
    // Skip header
    std::getline(f, line);

    while (std::getline(f, line)) {
        std::stringstream ss(line);
        std::string temp;
        std::string local_addr;
        unsigned long inode = 0;
        
        // Format: sl local_address rem_address st tx_queue rx_queue tr tm->when retrnsmt   uid  timeout inode
        // Example: 0: 0100007F:1EB6 00000000:0000 0A 00000000:00000000 00:00000000 00000000     0        0 12345
        
        ss >> temp; // sl
        ss >> local_addr; // local_address (hex IP:hex Port)
        
        // Parse port
        size_t colon = local_addr.find(':');
        if (colon != std::string::npos) {
            try {
                int port = std::stoi(local_addr.substr(colon + 1), nullptr, 16);
                
                // Skip rem_address, st, tx_queue, rx_queue, tr, tm->when, retrnsmt, uid, timeout
                for (int i = 0; i < 8; ++i) ss >> temp;
                
                ss >> inode;
                
                if (inode != 0) {
                    map[port] = inode;
                }
            } catch (...) {}
        }
    }
    return map;
}

// 根据源端口获取进程路径 (Linux 实现)
std::string Platform::getProcessPath(int port) {
    // 1. 获取端口对应的 inode
    unsigned long target_inode = 0;
    
    auto tcp_map = getTcpPortInodeMap("/proc/net/tcp");
    if (tcp_map.count(port)) {
        target_inode = tcp_map[port];
    } else {
        auto tcp6_map = getTcpPortInodeMap("/proc/net/tcp6");
        if (tcp6_map.count(port)) {
            target_inode = tcp6_map[port];
        }
    }

    if (target_inode == 0) return "";

    // 2. 遍历 /proc/[pid]/fd/ 寻找匹配的 inode
    DIR* proc_dir = opendir("/proc");
    if (!proc_dir) return "";

    struct dirent* entry;
    while ((entry = readdir(proc_dir)) != nullptr) {
        if (!isdigit(entry->d_name[0])) continue;

        std::string pid = entry->d_name;
        std::string fd_dir_path = "/proc/" + pid + "/fd";
        
        DIR* fd_dir = opendir(fd_dir_path.c_str());
        if (!fd_dir) continue;

        struct dirent* fd_entry;
        bool found = false;
        while ((fd_entry = readdir(fd_dir)) != nullptr) {
            if (fd_entry->d_type != DT_LNK) continue;

            std::string fd_path = fd_dir_path + "/" + fd_entry->d_name;
            char target[256];
            ssize_t len = readlink(fd_path.c_str(), target, sizeof(target) - 1);
            if (len > 0) {
                target[len] = '\0';
                std::string target_str(target);
                // socket:[inode]
                if (target_str.find("socket:[") == 0) {
                    try {
                        unsigned long inode = std::stoul(target_str.substr(8, target_str.length() - 9));
                        if (inode == target_inode) {
                            found = true;
                            break;
                        }
                    } catch (...) {}
                }
            }
        }
        closedir(fd_dir);

        if (found) {
            // 3. 获取 exe 路径
            std::string exe_link = "/proc/" + pid + "/exe";
            char exe_path[1024];
            ssize_t len = readlink(exe_link.c_str(), exe_path, sizeof(exe_path) - 1);
            if (len > 0) {
                exe_path[len] = '\0';
                closedir(proc_dir);
                return std::string(exe_path);
            }
        }
    }
    closedir(proc_dir);

    return "";
}

} // namespace common
} // namespace clash
