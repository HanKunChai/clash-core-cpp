#pragma once

#include "adapter/proxy_adapter.h"
#include "dns/resolver.h"
#include <string>
#include <memory>
#include <asio.hpp>

namespace clash
{
    namespace adapter
    {
        /**
         * @brief Shadowsocks 协议适配器
         * 
         * 实现 Shadowsocks 协议，支持多种加密方式。
         */
        class ShadowsocksAdapter : public ProxyAdapter, public std::enable_shared_from_this<ShadowsocksAdapter>
        {
        public:
            /**
             * @brief Shadowsocks 配置选项
             */
            struct Option
            {
                std::string name;
                std::string server;
                int port;
                std::string password;
                std::string cipher;
                bool udp = false;
            };

            /**
             * @brief 构造函数
             * 
             * @param option 配置选项
             */
            ShadowsocksAdapter(Option option);

            std::string name() const override;
            constant::AdapterType type() const override;

            /**
             * @brief 发起连接
             * 
             * 连接到 Shadowsocks 服务器，并建立加密通道。
             * 
             * @param metadata 连接元数据
             * @param io_context IO 上下文
             * @param handler 连接回调
             */
            void dial(const constant::Metadata& metadata, asio::io_context& io_context, ConnectHandler handler) override;

        private:
            Option option_;
        };

    } // namespace adapter
} // namespace clash

