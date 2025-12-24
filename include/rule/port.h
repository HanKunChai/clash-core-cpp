#pragma once

#include "rule/rule_interface.h"
#include <string>

namespace clash {
namespace rule {

class Port : public Rule {
public:
    enum class Type {
        SrcPort,
        DstPort,
        InboundPort
    };

    Port(Type type, int port, std::string adapter)
        : type_(type), port_(port), adapter_(std::move(adapter)) {}

    constant::RuleType type() const override {
        switch (type_) {
            case Type::SrcPort: return constant::RuleType::SrcPort;
            case Type::DstPort: return constant::RuleType::DstPort;
            case Type::InboundPort: return constant::RuleType::InboundPort;
        }
        return constant::RuleType::DstPort;
    }

    // 匹配逻辑：检查端口是否匹配
    bool match(const constant::Metadata& metadata) const override {
        switch (type_) {
            case Type::SrcPort:
                return metadata.srcPort == port_;
            case Type::DstPort:
                return metadata.dstPort == port_;
            // InboundPort not fully supported in metadata yet, assuming 0 or need update
            case Type::InboundPort:
                return false; // TODO: Add inbound port to metadata
        }
        return false;
    }

    std::string adapter() const override { return adapter_; }
    std::string payload() const override { return std::to_string(port_); }

private:
    Type type_;
    int port_;
    std::string adapter_;
};

} // namespace rule
} // namespace clash
