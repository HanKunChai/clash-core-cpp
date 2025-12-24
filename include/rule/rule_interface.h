#pragma once

#include <string>
#include "constant/rule.h"
#include "constant/metadata.h"

namespace clash {
namespace rule {

class Rule {
public:
    virtual ~Rule() = default;

    virtual constant::RuleType type() const = 0;
    virtual bool match(const constant::Metadata& metadata) const = 0;
    virtual std::string adapter() const = 0;
    virtual std::string payload() const = 0;
    virtual bool shouldResolveIP() const { return false; }
};

} // namespace rule
} // namespace clash
