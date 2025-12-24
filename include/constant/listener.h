#pragma once

#include <string>

namespace clash {
namespace constant {

enum class InboundType {
    Socks,
    Redir,
    Tproxy,
    HTTP,
    Mixed
};

struct Inbound {
    InboundType type;
    std::string bindAddress;
    bool isFromPortCfg = false;
    // Additional fields for specific inbounds (port, user, pass, etc.)
    // In Go, these are handled by dynamic decoding or specific structs.
    // For C++, we might need a variant or subclassing, or just a map of options.
    int port = 0;
};

} // namespace constant
} // namespace clash
