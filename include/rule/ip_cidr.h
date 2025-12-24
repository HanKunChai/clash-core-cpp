#pragma once

#include "rule/rule_interface.h"
#include <asio.hpp>
#include <string>

namespace clash {
namespace rule {

class IPCIDR : public Rule {
public:
    IPCIDR(std::string cidr, std::string adapter, bool noResolve = false);

    constant::RuleType type() const override;
    bool match(const constant::Metadata& metadata) const override;
    std::string adapter() const override;
    std::string payload() const override;
    bool shouldResolveIP() const override;

protected:
    std::string cidr_;
    std::string adapter_;
    bool noResolve_;
    
    asio::ip::address network_address_;
    int prefix_length_;

    bool match_v4(const asio::ip::address_v4& addr) const;
    bool match_v6(const asio::ip::address_v6& addr) const;
};

} // namespace rule
} // namespace clash
