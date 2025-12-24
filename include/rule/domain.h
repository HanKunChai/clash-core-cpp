#pragma once

#include "rule/rule_interface.h"
#include <algorithm>
#include <string>

namespace clash {
namespace rule {

class Domain : public Rule {
public:
    Domain(std::string domain, std::string adapter)
        : domain_(std::move(domain)), adapter_(std::move(adapter)) {
        std::transform(domain_.begin(), domain_.end(), domain_.begin(), ::tolower);
    }

    constant::RuleType type() const override { return constant::RuleType::Domain; }
    
    bool match(const constant::Metadata& metadata) const override {
        if (metadata.host.empty()) return false;
        // Simple case-insensitive comparison (assuming metadata.host is already lowercased or we do it here)
        // For performance, we should ensure metadata.host is lowercased once.
        // Here we just do a simple check.
        std::string host = metadata.host;
        std::transform(host.begin(), host.end(), host.begin(), ::tolower);
        return host == domain_;
    }

    std::string adapter() const override { return adapter_; }
    std::string payload() const override { return domain_; }

private:
    std::string domain_;
    std::string adapter_;
};

class DomainSuffix : public Rule {
public:
    DomainSuffix(std::string suffix, std::string adapter)
        : suffix_(std::move(suffix)), adapter_(std::move(adapter)) {
        std::transform(suffix_.begin(), suffix_.end(), suffix_.begin(), ::tolower);
    }

    constant::RuleType type() const override { return constant::RuleType::DomainSuffix; }

    bool match(const constant::Metadata& metadata) const override {
        if (metadata.host.empty()) return false;
        std::string host = metadata.host;
        std::transform(host.begin(), host.end(), host.begin(), ::tolower);

        if (host.length() < suffix_.length()) return false;
        
        // Exact match
        if (host == suffix_) return true;
        
        // Suffix match (must be preceded by dot)
        if (host.length() > suffix_.length()) {
            if (host.compare(host.length() - suffix_.length(), suffix_.length(), suffix_) == 0) {
                return host[host.length() - suffix_.length() - 1] == '.';
            }
        }
        return false;
    }

    std::string adapter() const override { return adapter_; }
    std::string payload() const override { return suffix_; }

private:
    std::string suffix_;
    std::string adapter_;
};

} // namespace rule
} // namespace clash
