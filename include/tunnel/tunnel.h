#pragma once

#include <map>
#include <vector>
#include <string>
#include <memory>
#include "adapter/proxy_adapter.h"
#include "rule/rule_interface.h"
#include "constant/metadata.h"
#include "tunnel/mode.h"

namespace clash {
namespace dns { class Resolver; }

namespace tunnel {

class Tunnel {
public:
    Tunnel();
    
    void addProxy(std::shared_ptr<adapter::ProxyAdapter> proxy);
    std::shared_ptr<adapter::ProxyAdapter> proxy(const std::string& name);
    void setRules(std::vector<std::shared_ptr<rule::Rule>> rules);
    void setMode(Mode mode);
    void setResolver(std::shared_ptr<dns::Resolver> resolver);

    std::shared_ptr<adapter::ProxyAdapter> match(const constant::Metadata& metadata);

    const std::map<std::string, std::shared_ptr<adapter::ProxyAdapter>>& proxies() const { return proxies_; }

private:
    std::map<std::string, std::shared_ptr<adapter::ProxyAdapter>> proxies_;
    std::vector<std::shared_ptr<rule::Rule>> rules_;
    Mode mode_ = Mode::Rule;
    std::shared_ptr<dns::Resolver> resolver_;
};


} // namespace tunnel
} // namespace clash
