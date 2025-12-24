#pragma once

#include "adapter/proxy_adapter.h"
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <atomic>

namespace clash
{
namespace adapter
{

// LoadBalanceAdapter: 负载均衡适配器
// 在一组代理之间分发流量
class LoadBalanceAdapter : public ProxyAdapter
{
public:
    // 负载均衡策略
    enum class Strategy
    {
        RoundRobin,       // 轮询
        ConsistentHashing // 一致性哈希
    };

    // 构造函数
    LoadBalanceAdapter(std::string name, Strategy strategy);

    std::string name() const override;
    constant::AdapterType type() const override;

    // 发起连接
    // 根据策略选择一个代理进行连接
    void dial(const constant::Metadata& metadata, asio::io_context& io_context, ConnectHandler handler) override;

    // 设置代理列表
    void setProxies(std::vector<std::shared_ptr<ProxyAdapter>> proxies);

private:
    std::string name_;
    Strategy strategy_;
    std::vector<std::shared_ptr<ProxyAdapter>> proxies_;
    std::atomic<size_t> cursor_{0};
    std::mutex mutex_;
};

} // namespace adapter
} // namespace clash

