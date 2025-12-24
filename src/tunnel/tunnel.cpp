#include "tunnel/tunnel.h"
#include "log/log.h"
#include "adapter/direct.h"
#include "adapter/reject.h"
#include "dns/resolver.h"

namespace clash {
namespace tunnel {

// 构造函数
// 初始化 Tunnel 核心组件
// 默认注册内置的 DIRECT (直连) 和 REJECT (拒绝) 策略
Tunnel::Tunnel() {
    // 注册内置策略
    addProxy(std::make_shared<adapter::DirectAdapter>());
    addProxy(std::make_shared<adapter::RejectAdapter>());
}

// 添加代理策略
// 将解析配置得到的代理或代理组加入到管理列表中
void Tunnel::addProxy(std::shared_ptr<adapter::ProxyAdapter> proxy) {
    proxies_[proxy->name()] = proxy;
}

// 获取指定名称的代理策略
std::shared_ptr<adapter::ProxyAdapter> Tunnel::proxy(const std::string& name) {
    auto it = proxies_.find(name);
    if (it != proxies_.end()) {
        return it->second;
    }
    return nullptr;
}

// 更新路由规则列表
void Tunnel::setRules(std::vector<std::shared_ptr<rule::Rule>> rules) {
    rules_ = std::move(rules);
}

// 设置运行模式
// Mode::Global: 全局代理
// Mode::Rule: 规则分流
// Mode::Direct: 全局直连
void Tunnel::setMode(Mode mode) {
    mode_ = mode;
}

// 设置 DNS 解析器
// 用于后续可能的域名解析需求
void Tunnel::setResolver(std::shared_ptr<dns::Resolver> resolver) {
    resolver_ = std::move(resolver);
}

// 核心路由匹配函数
// 根据连接的元数据 (Metadata) 决定流量的去向
// 返回值：匹配到的代理适配器 (ProxyAdapter)
std::shared_ptr<adapter::ProxyAdapter> Tunnel::match(const constant::Metadata& metadata) {
    // 1. 全局模式 (Global Mode)
    // 强制所有流量走 GLOBAL 代理组
    // 如果未定义 GLOBAL，则回退到 DIRECT
    if (mode_ == Mode::Global) {
        auto it = proxies_.find("GLOBAL");
        if (it != proxies_.end()) {
            return it->second;
        }
        // Fallback to DIRECT if GLOBAL not found (or handle as error)
        return proxies_["DIRECT"];
    }

    // 2. 直连模式 (Direct Mode)
    // 所有流量直接连接，不经过任何代理
    if (mode_ == Mode::Direct) {
        return proxies_["DIRECT"];
    }

    // 3. 规则模式 (Rule Mode)
    // 按顺序遍历规则列表，一旦匹配成功即停止
    for (const auto& rule : rules_) {
        if (rule->match(metadata)) {
            std::string adapterName = rule->adapter();
            log::debug("Match rule: {} -> {}", rule->payload(), adapterName);
            
            auto it = proxies_.find(adapterName);
            if (it != proxies_.end()) {
                return it->second;
            } else {
                // 如果规则指向的代理不存在，降级为 DIRECT 并警告
                log::warn("Proxy not found: {}, using DIRECT", adapterName);
                return proxies_["DIRECT"];
            }
        }
    }

    // 4. 默认兜底 (Final)
    // 如果没有规则匹配，默认走 DIRECT (通常配置文件的最后一条规则是 MATCH -> Proxy)
    // 但如果连 MATCH 规则都没有，这里作为最后的保障
    log::debug("No rule matched, using DIRECT");
    return proxies_["DIRECT"];
}

} // namespace tunnel
} // namespace clash
