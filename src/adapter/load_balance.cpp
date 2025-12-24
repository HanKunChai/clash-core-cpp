#include "adapter/load_balance.h"
#include <iostream>

namespace clash
{
namespace adapter
{

// 构造函数
// 初始化负载均衡适配器，指定名称和负载均衡策略
LoadBalanceAdapter::LoadBalanceAdapter(std::string name, Strategy strategy)
    : name_(std::move(name)), strategy_(strategy)
{
}

// 获取适配器名称
std::string LoadBalanceAdapter::name() const
{
    return name_;
}

// 获取适配器类型
constant::AdapterType LoadBalanceAdapter::type() const
{
    return constant::AdapterType::LoadBalance;
}

// 设置子代理列表
// 更新参与负载均衡的代理节点集合
void LoadBalanceAdapter::setProxies(std::vector<std::shared_ptr<ProxyAdapter>> proxies)
{
    std::lock_guard<std::mutex> lock(mutex_);
    proxies_ = std::move(proxies);
}

// 发起连接
// 根据配置的负载均衡策略（轮询或一致性哈希）选择一个子代理进行连接
void LoadBalanceAdapter::dial(const constant::Metadata& metadata, asio::io_context& io_context, ConnectHandler handler)
{
    std::shared_ptr<ProxyAdapter> proxy;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (proxies_.empty())
        {
            // 如果没有可用的代理，返回错误
            handler(std::make_error_code(std::errc::destination_address_required), nullptr);
            return;
        }

        if (strategy_ == Strategy::RoundRobin)
        {
            // 轮询策略 (Round Robin)
            // 使用原子计数器 cursor_ 依次选择下一个代理
            size_t index = cursor_.fetch_add(1) % proxies_.size();
            proxy = proxies_[index];
        }
        else if (strategy_ == Strategy::ConsistentHashing)
        {
            // 一致性哈希策略 (Consistent Hashing)
            // 这里实现了一个简单的基于源 IP 的哈希选择
            // 保证来自同一个 IP 的请求总是被转发到同一个代理，有助于保持会话状态
            
            if (metadata.srcIP.empty())
            {
                // 如果源 IP 为空（例如本地请求可能没有源 IP），回退到轮询策略
                size_t index = cursor_.fetch_add(1) % proxies_.size();
                proxy = proxies_[index];
            }
            else
            {
                // 计算源 IP 的哈希值并取模
                std::hash<std::string> hasher;
                size_t hash = hasher(metadata.srcIP);
                size_t index = hash % proxies_.size();
                proxy = proxies_[index];
            }
        }
    }

    if (proxy)
    {
        // 转发连接请求给选中的代理
        proxy->dial(metadata, io_context, std::move(handler));
    }
    else
    {
         handler(std::make_error_code(std::errc::destination_address_required), nullptr);
    }
}

} // namespace adapter
} // namespace clash
