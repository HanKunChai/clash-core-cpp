#pragma once

#include "adapter/proxy_adapter.h"
#include <vector>
#include <string>
#include <memory>
#include <asio.hpp>
#include <deque>
#include <mutex>

namespace clash
{
    namespace adapter
    {
        /**
         * @brief 自动测速选择适配器
         * 
         * 定期测试一组代理的延迟，并自动选择延迟最低的代理。
         */
        class URLTestAdapter : public ProxyAdapter, public std::enable_shared_from_this<URLTestAdapter>
        {
        public:
            /**
             * @brief 构造函数
             * 
             * @param name 适配器名称
             * @param url 用于测速的 URL
             * @param interval 测速间隔（秒）
             * @param io_context IO 上下文
             */
            URLTestAdapter(std::string name, std::string url, int interval, asio::io_context& io_context);

            std::string name() const override;
            constant::AdapterType type() const override;

            /**
             * @brief 发起连接
             * 
             * 使用当前测速最快的代理进行连接。
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
             * @brief 启动测速任务
             */
            void start();

        private:
            /**
             * @brief 执行一次测速
             */
            void runTest();
            
            /**
             * @brief 测速完成回调
             * 
             * @param proxyName 代理名称
             * @param latency 延迟（毫秒）
             */
            void onTestComplete(const std::string& proxyName, int latency);

            std::string name_;
            std::string url_;
            int interval_;
            asio::io_context& io_context_;
            asio::steady_timer timer_;

            std::vector<std::shared_ptr<ProxyAdapter>> proxies_;
            std::shared_ptr<ProxyAdapter> fastest_;
            int min_latency_ = -1;
            
            std::mutex mutex_;
        };

    } // namespace adapter
} // namespace clash

