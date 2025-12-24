#pragma once

#include "rule/rule_interface.h"
#include <algorithm>
#include <string>

namespace clash {
namespace rule {

class DomainKeyword : public Rule {
public:
    DomainKeyword(std::string keyword, std::string adapter)
        : keyword_(std::move(keyword)), adapter_(std::move(adapter)) {
        std::transform(keyword_.begin(), keyword_.end(), keyword_.begin(), ::tolower);
    }

    constant::RuleType type() const override { return constant::RuleType::DomainKeyword; }

    // 匹配逻辑：检查域名是否包含关键字
    bool match(const constant::Metadata& metadata) const override {
        if (metadata.host.empty()) return false;
        std::string host = metadata.host;
        std::transform(host.begin(), host.end(), host.begin(), ::tolower);

        return host.find(keyword_) != std::string::npos;
    }

    std::string adapter() const override { return adapter_; }
    std::string payload() const override { return keyword_; }

private:
    std::string keyword_;
    std::string adapter_;
};

} // namespace rule
} // namespace clash
