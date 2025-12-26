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
        /**
         * @brief 故障转移适配器
         * 
         * 按顺序测试代理，选择第一个可用的代理。
         * 适用于高可用性场景，当主节点挂掉时自动切换到备用节点。
         */
        class FallbackAdapter : public ProxyAdapter, public std::enable_shared_from_this<FallbackAdapter>
        {
        public:
            /**
             * @brief 构造函数
             * 
             * @param name 适配器名称
             * @param url 测试 URL
             * @param interval 测试间隔（秒）
             * @param io_context IO 上下文
             */
            FallbackAdapter(std::string name, std::string url, int interval, asio::io_context& io_context);

            std::string name() const override;
            constant::AdapterType type() const override;

            /**
             * @brief 发起连接
             * 
             * 使用当前选中的可用代理。
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
            
            /**
             * @brief 启动健康检查
             */
            void start();

        private:
            /**
             * @brief 执行健康检查
             */
            void runTest();
            
            /**
             * @brief 检查完成回调
             * 
             * @param proxyName 代理名称
             * @param latency 延迟（毫秒）
             */
            void onTestComplete(const std::string& proxyName, int latency);
            
            /**
             * @brief 更新当前选择
             */
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

