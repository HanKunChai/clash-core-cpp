#pragma once

#include <string>

namespace clash {
namespace constant {

enum class RuleType {
    Domain,
    DomainSuffix,
    DomainKeyword,
    GEOIP,
    IPCIDR,
    SrcIPCIDR,
    SrcPort,
    DstPort,
    InboundPort,
    Process,
    ProcessPath,
    IPSet,
    MATCH
};

} // namespace constant
} // namespace clash
