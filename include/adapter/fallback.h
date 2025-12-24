#pragma once

#include "adapter/proxy_adapter.h"
#include <vector>
#include <string>
#include <memory>
#include <asio.hpp>
#include <mutex>
#include <map>

namespace clash
{
namespace adapter
{

// FallbackAdapter: 故障转移适配器
// 按顺序测试代理，选择第一个可用的代理
class FallbackAdapter : public ProxyAdapter, public std::enable_shared_from_this<FallbackAdapter>
{
public:
    // 构造函数
    FallbackAdapter(std::string name, std::string url, int interval, asio::io_context& io_context);

    std::string name() const override;
    constant::AdapterType type() const override;

    // 发起连接
    // 使用当前选中的可用代理
    void dial(const constant::Metadata& metadata, asio::io_context& io_context, ConnectHandler handler) override;

    // 设置代理列表
    void setProxies(std::vector<std::shared_ptr<ProxyAdapter>> proxies);
    
    // 启动健康检查
    void start();

private:
    // 执行健康检查
    void runTest();
    
    // 检查完成回调
    void onTestComplete(const std::string& proxyName, int latency);
    
    // 更新当前选择
    void updateSelection();

    std::string name_;
    std::string url_;
    int interval_;
    asio::io_context& io_context_;
    asio::steady_timer timer_;

    std::vector<std::shared_ptr<ProxyAdapter>> proxies_;
    std::shared_ptr<ProxyAdapter> selected_;
    std::map<std::string, int> latencies_; // 存储每个代理的延迟
    
    std::mutex mutex_;
};

} // namespace adapter
} // namespace clash

