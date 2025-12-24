#pragma once

#include "rule/rule_interface.h"

namespace clash {
namespace rule {

class Final : public Rule {
public:
    Final(std::string adapter) : adapter_(std::move(adapter)) {}

    constant::RuleType type() const override { return constant::RuleType::MATCH; }
    bool match(const constant::Metadata& /*metadata*/) const override { return true; }
    std::string adapter() const override { return adapter_; }
    std::string payload() const override { return ""; }

private:
    std::string adapter_;
};

} // namespace rule
} // namespace clash
