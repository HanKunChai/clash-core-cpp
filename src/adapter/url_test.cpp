#include "adapter/url_test.h"
#include "log/log.h"
#include <iostream>
#include <chrono>

namespace clash
{
    namespace adapter
    {

        // 构造函数
        // 初始化 URLTest 适配器，设置测试 URL 和间隔时间
        URLTestAdapter::URLTestAdapter(std::string name, std::string url, int interval, asio::io_context& io_context)
            : name_(std::move(name)), url_(std::move(url)), interval_(interval), io_context_(io_context), timer_(io_context)
        {
            if (interval_ <= 0)
            {
                interval_ = 300; // 默认 300 秒
            }
        }

        std::string URLTestAdapter::name() const
        {
            return name_;
        }

        constant::AdapterType URLTestAdapter::type() const
        {
            return constant::AdapterType::URLTest;
        }

        // 路由选择
        // 将流量转发给当前测速最快（延迟最低）的代理节点
        void URLTestAdapter::dial(const constant::Metadata& metadata, asio::io_context& io_context, ConnectHandler handler)
        {
            std::shared_ptr<ProxyAdapter> target;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                target = fastest_;
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
        // 更新参与测速的代理节点集合
        void URLTestAdapter::setProxies(std::vector<std::shared_ptr<ProxyAdapter>> proxies)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            proxies_ = std::move(proxies);
            if (!proxies_.empty())
            {
                fastest_ = proxies_[0]; // 默认使用第一个，直到第一次测速完成
            }
        }

        // 启动测速循环
        void URLTestAdapter::start()
        {
            runTest();
        }

        // 执行测速任务
        // 1. 解析测试 URL
        // 2. 对所有代理节点并发发起连接测试
        // 3. 记录连接耗时
        // 4. 调度下一次测试
        void URLTestAdapter::runTest()
        {
            auto self = shared_from_this();

            // 解析测试 URL (简化实现：假设是 http://host:port 或 http://host)
            // 实际生产环境应使用健壮的 URL 解析器
            std::string host = "www.gstatic.com";
            int port = 80;

            if (!url_.empty())
            {
                // 简单的 URL 解析逻辑
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

            LOG_DEBUG("URLTest '%s' starting test to %s:%d", name_.c_str(), host.c_str(), port);

            // 对每个代理发起连接测试
            // 注意：这里通过并发发起 dial 请求来模拟延迟测试
            // 实际上应该发送 HTTP HEAD 请求并检查响应码
            // 这里简化为 TCP 连接建立时间

            // 获取代理列表快照，避免在遍历时被修改
            std::vector<std::shared_ptr<ProxyAdapter>> current_proxies;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                current_proxies = proxies_;
                min_latency_ = -1; // 重置最小延迟
            }

            for (const auto& proxy : current_proxies)
            {
                auto start_time = std::chrono::steady_clock::now();
                std::string p_name = proxy->name();

                // 使用 dial 进行测试
                proxy->dial(metadata, io_context_,
                    [this, self, p_name, start_time](std::error_code ec, std::unique_ptr<common::Connection> conn)
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
                            // LOG_DEBUG("URLTest proxy %s failed: %s", p_name.c_str(), ec.message().c_str());
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
        // 比较延迟，更新最快的代理节点
        void URLTestAdapter::onTestComplete(const std::string& proxyName, int latency)
        {
            std::lock_guard<std::mutex> lock(mutex_);

            // LOG_DEBUG("URLTest '%s' proxy '%s' latency: %dms", name_.c_str(), proxyName.c_str(), latency);

            // 如果是第一个结果，或者比当前最小延迟更小，则更新
            if (min_latency_ == -1 || latency < min_latency_)
            {
                min_latency_ = latency;
                // 查找对应的代理指针
                for (const auto& p : proxies_)
                {
                    if (p->name() == proxyName)
                    {
                        fastest_ = p;
                        LOG_INFO("URLTest '%s' switched to '%s' (latency: %dms)", name_.c_str(), proxyName.c_str(), latency);
                        break;
                    }
                }
            }
        }

    } // namespace adapter
} // namespace clash

