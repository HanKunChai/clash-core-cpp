#pragma once

#include "adapter/proxy_adapter.h"
#include "adapter/ssr_protocol.h"
#include <string>
#include <memory>
#include <asio.hpp>

namespace clash
{
    namespace adapter
    {
        /**
         * @brief ShadowsocksR 协议适配器
         * 
         * 实现 ShadowsocksR 协议。
         */
        class ShadowsocksRAdapter : public ProxyAdapter, public std::enable_shared_from_this<ShadowsocksRAdapter>
        {
        public:
            /**
             * @brief ShadowsocksR 配置选项
             */
            struct Option
            {
                std::string name;
                std::string server;
                int port;
                std::string password;
                std::string cipher;
                std::string protocol;
                std::string protocol_param;
                std::string obfs;
                std::string obfs_param;
                bool udp = false;
            };

            /**
             * @brief 构造函数
             * 
             * @param option 配置选项
             */
            ShadowsocksRAdapter(Option option);

            std::string name() const override;
            constant::AdapterType type() const override;

            /**
             * @brief 发起连接
             * 
             * @param metadata 连接元数据
             * @param io_context IO 上下文
             * @param handler 连接回调
             */
            void dial(const constant::Metadata& metadata, asio::io_context& io_context, ConnectHandler handler) override;

        private:
            Option option_;
        };
    }
}
