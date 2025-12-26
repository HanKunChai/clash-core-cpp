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
        /**
         * @brief 负载均衡适配器
         * 
         * 在一组代理之间分发流量。
         */
        class LoadBalanceAdapter : public ProxyAdapter
        {
        public:
            /**
             * @brief 负载均衡策略
             */
            enum class Strategy
            {
                RoundRobin,       // 轮询
                ConsistentHashing // 一致性哈希
            };

            /**
             * @brief 构造函数
             * 
             * @param name 适配器名称
             * @param strategy 负载均衡策略
             */
            LoadBalanceAdapter(std::string name, Strategy strategy);

            std::string name() const override;
            constant::AdapterType type() const override;

            /**
             * @brief 发起连接
             * 
             * 根据策略选择一个代理进行连接。
             * 
             * @param metadata 连接元数据
             * @param io_context IO 上下文
             * @param handler 连接回调
             */
            void dial(const constant::Metadata& metadata, asio::io_context& io_context, ConnectHandler handler) override;

            /**
             * @brief 设置代理列表
             * 
             * @param proxies 代理列表
             */
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

