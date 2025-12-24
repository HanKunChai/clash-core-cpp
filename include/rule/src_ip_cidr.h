#pragma once

#include "rule/ip_cidr.h"

namespace clash {
namespace rule {

class SrcIPCIDR : public IPCIDR {
public:
    SrcIPCIDR(std::string cidr, std::string adapter)
        : IPCIDR(std::move(cidr), std::move(adapter), true) {} // SrcIP never needs DNS resolution

    constant::RuleType type() const override { return constant::RuleType::SrcIPCIDR; }

    bool match(const constant::Metadata& metadata) const override {
        if (metadata.srcIP.empty()) return false;

        std::error_code ec;
        auto src_addr = asio::ip::make_address(metadata.srcIP, ec);
        if (ec) return false;

        if (src_addr.is_v4() && network_address_.is_v4()) {
            return match_v4(src_addr.to_v4());
        } else if (src_addr.is_v6() && network_address_.is_v6()) {
            return match_v6(src_addr.to_v6());
        }

        return false;
    }
    
    bool shouldResolveIP() const override { return false; }
};

} // namespace rule
} // namespace clash
