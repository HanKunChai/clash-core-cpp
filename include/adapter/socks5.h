#pragma once

#include "adapter/proxy_adapter.h"
#include <string>

namespace clash
{
    namespace adapter
    {
        /**
         * @brief Socks5 协议适配器
         * 
         * 实现标准的 Socks5 客户端协议，支持无认证和用户名/密码认证。
         */
        class Socks5Adapter : public ProxyAdapter, public std::enable_shared_from_this<Socks5Adapter>
        {
        public:
            /**
             * @brief Socks5 配置选项
             */
            struct Option
            {
                std::string name;
                std::string server;
                int port;
                std::string username;
                std::string password;
            };

            /**
             * @brief 构造函数
             * 
             * @param option 配置选项
             */
            Socks5Adapter(Option option);

            /**
             * @brief 获取代理名称
             * 
             * @return std::string 代理名称
             */
            std::string name() const override;
            
            /**
             * @brief 获取代理类型
             * 
             * @return constant::AdapterType 代理类型 Socks5
             */
            constant::AdapterType type() const override;

            /**
             * @brief 发起连接
             * 
             * 连接到 Socks5 代理服务器，完成握手，并连接到目标主机。
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

