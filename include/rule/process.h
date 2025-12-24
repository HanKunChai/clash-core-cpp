#pragma once

#include "rule/rule_interface.h"
#include <string>

namespace clash {
namespace rule {

class Process : public Rule {
public:
    Process(std::string processName, std::string adapter);

    constant::RuleType type() const override;
    bool match(const constant::Metadata& metadata) const override;
    std::string adapter() const override;
    std::string payload() const override;

private:
    std::string processName_;
    std::string adapter_;
};

class ProcessPath : public Rule {
public:
    ProcessPath(std::string processPath, std::string adapter);

    constant::RuleType type() const override;
    bool match(const constant::Metadata& metadata) const override;
    std::string adapter() const override;
    std::string payload() const override;

private:
    std::string processPath_;
    std::string adapter_;
};

} // namespace rule
} // namespace clash
