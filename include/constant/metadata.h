#pragma once

#include <string>
#include <optional>
#include "constant/adapters.h"

namespace clash {
namespace constant {

struct Metadata {
    enum class Type {
        Socks5,
        Http,
        Redir,
        Tproxy
    };

    Type type = Type::Socks5;
    std::string srcIP;
    int srcPort = 0;
    std::string dstIP;
    int dstPort = 0;
    std::string host;
    
    // Helper to get effective destination address
    std::string destination() const {
        if (!host.empty()) return host;
        return dstIP;
    }
};

} // namespace constant
} // namespace clash
