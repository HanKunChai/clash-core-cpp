#pragma once

#include <string>
#include <vector>

namespace clash {
namespace constant {

enum class AdapterType {
    Direct,
    Reject,
    Shadowsocks,
    ShadowsocksR,
    Snell,
    Socks5,
    Http,
    Vmess,
    Trojan,
    Relay,
    Selector,
    URLTest,
    Fallback,
    LoadBalance,
};

using Chain = std::vector<std::string>;

} // namespace constant
} // namespace clash
