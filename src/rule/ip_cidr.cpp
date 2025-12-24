#include "rule/ip_cidr.h"
#include "log/log.h"
#include <vector>
#include <sstream>
#include <cmath>

namespace clash {
namespace rule {

// 构造函数：解析 CIDR 字符串 (例如 "192.168.1.0/24")
IPCIDR::IPCIDR(std::string cidr, std::string adapter, bool noResolve)
    : cidr_(std::move(cidr)), adapter_(std::move(adapter)), noResolve_(noResolve) {
    
    auto pos = cidr_.find('/');
    std::string ip_str;
    
    if (pos != std::string::npos) {
        ip_str = cidr_.substr(0, pos);
        try {
            prefix_length_ = std::stoi(cidr_.substr(pos + 1));
        } catch (...) {
            prefix_length_ = 0;
        }
    } else {
        ip_str = cidr_;
        prefix_length_ = -1; // 标记未指定前缀
    }

    std::error_code ec;
    auto addr = asio::ip::make_address(ip_str, ec);
    if (!ec) {
        network_address_ = addr;
        if (prefix_length_ == -1) {
            prefix_length_ = network_address_.is_v4() ? 32 : 128;
        }
    } else {
        log::warn("Invalid IP in CIDR rule: {}", ip_str);
    }
}

constant::RuleType IPCIDR::type() const {
    return constant::RuleType::IPCIDR;
}

// 匹配逻辑：检查 Metadata 中的目标 IP 是否在 CIDR 范围内
bool IPCIDR::match(const constant::Metadata& metadata) const {
    // 如果 Metadata 中没有 IP，无法匹配 (除非我们在 Tunnel 中实现了 DNS 解析逻辑)
    // 目前假设 Metadata 可能包含 IP (如果是 SOCKS5 IP 请求) 或者 Tunnel 已经解析过
    if (metadata.dstIP.empty()) {
        return false;
    }

    std::error_code ec;
    auto dst_addr = asio::ip::make_address(metadata.dstIP, ec);
    if (ec) return false;

    if (dst_addr.is_v4() && network_address_.is_v4()) {
        return match_v4(dst_addr.to_v4());
    } else if (dst_addr.is_v6() && network_address_.is_v6()) {
        return match_v6(dst_addr.to_v6());
    }

    return false;
}

std::string IPCIDR::adapter() const {
    return adapter_;
}

std::string IPCIDR::payload() const {
    return cidr_;
}

bool IPCIDR::shouldResolveIP() const {
    return !noResolve_;
}

// IPv4 匹配辅助函数
bool IPCIDR::match_v4(const asio::ip::address_v4& addr) const {
    if (prefix_length_ == 0) return true;
    
    unsigned long mask = 0xFFFFFFFF;
    if (prefix_length_ < 32) {
        mask = mask << (32 - prefix_length_);
    }
    
    return (addr.to_ulong() & mask) == (network_address_.to_v4().to_ulong() & mask);
}

// IPv6 匹配辅助函数
bool IPCIDR::match_v6(const asio::ip::address_v6& addr) const {
    if (prefix_length_ == 0) return true;

    auto bytes_target = addr.to_bytes();
    auto bytes_net = network_address_.to_v6().to_bytes();
    
    int bytes_to_check = prefix_length_ / 8;
    int bits_remainder = prefix_length_ % 8;

    // 检查完整的字节
    for (int i = 0; i < bytes_to_check; ++i) {
        if (bytes_target[i] != bytes_net[i]) return false;
    }

    // 检查剩余的位
    if (bits_remainder > 0) {
        unsigned char mask = 0xFF << (8 - bits_remainder);
        if ((bytes_target[bytes_to_check] & mask) != (bytes_net[bytes_to_check] & mask)) {
            return false;
        }
    }

    return true;
}

} // namespace rule
} // namespace clash
