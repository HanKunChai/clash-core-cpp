#include "adapter/fallback.h"
#include "log/log.h"
#include <iostream>
#include <chrono>

namespace clash
{
    namespace adapter
    {

        // 构造函数
        // 初始化 Fallback 适配器，设置测试 URL 和间隔时间
        FallbackAdapter::FallbackAdapter(std::string name, std::string url, int interval, asio::io_context& io_context)
            : name_(std::move(name)), url_(std::move(url)), interval_(interval), io_context_(io_context), timer_(io_context)
        {
            if (interval_ <= 0)
            {
                interval_ = 300; // 默认 300 秒
            }
        }

        std::string FallbackAdapter::name() const
        {
            return name_;
        }

        constant::AdapterType FallbackAdapter::type() const
        {
            return constant::AdapterType::Fallback;
        }

        // 路由选择
        // 将流量转发给当前选中的（第一个可用的）代理节点
        void FallbackAdapter::dial(const constant::Metadata& metadata, asio::io_context& io_context, ConnectHandler handler)
        {
            std::shared_ptr<ProxyAdapter> target;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                target = selected_;
            }

            if (target)
            {
                target->dial(metadata, io_context, std::move(handler));
            }
            else
            {
                handler(std::make_error_code(std::errc::destination_address_required), nullptr);
            }
        }

        // 设置代理列表
        // 更新参与测速的代理节点集合，并重置延迟记录
        void FallbackAdapter::setProxies(std::vector<std::shared_ptr<ProxyAdapter>> proxies)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            proxies_ = std::move(proxies);
            latencies_.clear();
            if (!proxies_.empty())
            {
                selected_ = proxies_[0]; // 默认使用第一个
            }
        }

        // 启动测速循环
        void FallbackAdapter::start()
        {
            runTest();
        }

        // 执行测速任务
        // 1. 解析测试 URL
        // 2. 对所有代理节点并发发起连接测试
        // 3. 记录连接结果（成功/失败）
        // 4. 调度下一次测试
        void FallbackAdapter::runTest()
        {
            auto self = shared_from_this();

            // 解析测试 URL
            std::string host = "www.gstatic.com";
            int port = 80;

            if (!url_.empty())
            {
                size_t protocol = url_.find("://");
                size_t start = (protocol == std::string::npos) ? 0 : protocol + 3;
                size_t slash = url_.find('/', start);
                std::string host_port = (slash == std::string::npos) ? url_.substr(start) : url_.substr(start, slash - start);

                size_t colon = host_port.find(':');
                if (colon != std::string::npos)
                {
                    host = host_port.substr(0, colon);
                    port = std::stoi(host_port.substr(colon + 1));
                }
                else
                {
                    host = host_port;
                }
            }

            constant::Metadata metadata;
            metadata.type = constant::Metadata::Type::Http;
            metadata.host = host;
            metadata.dstPort = port;

            LOG_DEBUG("Fallback '%s' starting test to %s:%d", name_.c_str(), host.c_str(), port);

            std::vector<std::shared_ptr<ProxyAdapter>> current_proxies;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                current_proxies = proxies_;
            }

            for (const auto& proxy : current_proxies)
            {
                auto start_time = std::chrono::steady_clock::now();
                std::string p_name = proxy->name();

                proxy->dial(metadata, io_context_,
                    [this, self, p_name, start_time](std::error_code ec, std::shared_ptr<common::Connection> conn)
                    {
                        if (!ec)
                        {
                            auto end_time = std::chrono::steady_clock::now();
                            auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
                            if (conn)
                            {
                                conn->close();
                            }
                            onTestComplete(p_name, (int)latency);
                        }
                        else
                        {
                            onTestComplete(p_name, 0); // 0 表示失败或超时
                        }
                    });
            }

            // 调度下一次测试
            timer_.expires_after(std::chrono::seconds(interval_));
            timer_.async_wait([this, self](std::error_code ec)
            {
                if (!ec)
                {
                    runTest();
                }
            });
        }

        // 处理单个代理的测试结果
        // 记录延迟并触发选择更新
        void FallbackAdapter::onTestComplete(const std::string& proxyName, int latency)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            latencies_[proxyName] = latency;
            updateSelection();
        }

        // 更新选择策略
        // 遍历代理列表，选择第一个可用的（延迟 > 0）代理
        // 实现了故障转移逻辑：如果首选代理挂了，自动切换到下一个可用代理
        void FallbackAdapter::updateSelection()
        {
            for (const auto& proxy : proxies_)
            {
                auto it = latencies_.find(proxy->name());
                if (it != latencies_.end() && it->second > 0)
                {
                    if (selected_ != proxy)
                    {
                        LOG_INFO("Fallback '%s' switched to '%s'", name_.c_str(), proxy->name().c_str());
                        selected_ = proxy;
                    }
                    return;
                }
            }
            // 如果所有代理都不可用，保持当前选择或回退到第一个
            if (!proxies_.empty())
            {
                selected_ = proxies_[0];
            }
        }

    } // namespace adapter
} // namespace clash

