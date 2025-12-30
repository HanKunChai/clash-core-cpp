#pragma once

#include "adapter/proxy_adapter.h"
#include "log/log.h"

namespace clash
{
    namespace adapter
    {
        /**
         * @brief 直连适配器
         * 
         * 不经过任何代理，直接连接目标服务器。
         */
        class DirectAdapter : public ProxyAdapter, public std::enable_shared_from_this<DirectAdapter>
        {
        public:
            /**
             * @brief 获取代理名称
             * 
             * @return std::string 代理名称 "DIRECT"
             */
            std::string name() const override
            {
                return "DIRECT";
            }

            /**
             * @brief 获取代理类型
             * 
             * @return constant::AdapterType 代理类型 Direct
             */
            constant::AdapterType type() const override
            {
                return constant::AdapterType::Direct;
            }

            /**
             * @brief 发起连接
             * 
             * 1. 解析目标地址
             * 2. 直接建立 TCP 连接
             * 
             * @param metadata 连接元数据
             * @param io_context IO 上下文
             * @param handler 连接回调
             */
            void dial(const constant::Metadata& metadata, asio::io_context& io_context, ConnectHandler handler) override
            {
                auto socket = std::make_shared<asio::ip::tcp::socket>(io_context);
                auto socket_ptr = socket.get();
                
                // 使用 shared_ptr 管理 resolver 生命周期
                auto resolver = std::make_shared<asio::ip::tcp::resolver>(io_context);
                
                std::string host = metadata.destination();
                std::string port = std::to_string(metadata.dstPort);

                // 异步 DNS 解析
                resolver->async_resolve(host, port,
                    [this, socket, resolver, handler](std::error_code ec, asio::ip::tcp::resolver::results_type results) mutable
                    {
                        if (ec)
                        {
                            handler(ec, nullptr);
                            return;
                        }

                        auto s_ptr = socket.get();
                        // 异步连接目标 IP
                        asio::async_connect(*s_ptr, results,
                            [socket, handler](std::error_code ec, asio::ip::tcp::endpoint /*endpoint*/) mutable
                            {
                                if (ec)
                                {
                                    handler(ec, nullptr);
                                }
                                else
                                {
                                    // 连接成功，封装为 TcpConnection
                                    handler(ec, std::make_shared<common::TcpConnection>(std::move(*socket)));
                                }
                            });
                    });
            }
        };

    } // namespace adapter
} // namespace clash

