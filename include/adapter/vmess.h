#pragma once

#include "adapter/proxy_adapter.h"
#include <string>
#include <memory>
#include <asio.hpp>

namespace clash
{
    namespace adapter
    {
        /**
         * @brief VMess 协议适配器
         * 
         * V2Ray 的核心协议，支持多种传输方式（TCP, WebSocket 等）。
         */
        class VmessAdapter : public ProxyAdapter, public std::enable_shared_from_this<VmessAdapter>
        {
        public:
            /**
             * @brief VMess 配置选项
             */
            struct Option
            {
                std::string name;
                std::string server;
                int port;
                std::string uuid;
                int alterId = 0;
                std::string cipher;
                bool udp = false;
                bool tls = false;
                std::string network = "tcp"; // "tcp", "ws", "h2", "grpc"
                std::string wsPath;
                std::string wsHeaders;
            };

            /**
             * @brief 构造函数
             * 
             * @param option 配置选项
             */
            VmessAdapter(Option option);

            std::string name() const override;
            constant::AdapterType type() const override;

            /**
             * @brief 发起连接
             * 
             * 连接到 VMess 服务器。
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

