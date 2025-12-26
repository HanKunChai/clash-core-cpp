#include "adapter/vmess.h"
#include "log/log.h"
#include "common/connection.h"
#include <iostream>

namespace clash
{
    namespace adapter
    {

        // 辅助类：处理 VMess 连接建立
        // 目前仅实现了 TCP 连接建立，VMess 协议握手尚未实现
        class VmessDialer : public std::enable_shared_from_this<VmessDialer>
        {
        public:
            VmessDialer(const VmessAdapter::Option& option, const constant::Metadata& metadata, asio::io_context& io_context, ProxyAdapter::ConnectHandler handler)
                : option_(option), metadata_(metadata), socket_(io_context), handler_(std::move(handler)), resolver_(io_context)
            {
            }

            // 启动连接流程
            void start()
            {
                resolve_proxy();
            }

        private:
            // 1. 解析代理服务器地址
            // 使用异步 DNS 解析器解析 VMess 代理服务器的域名
            void resolve_proxy()
            {
                auto self = shared_from_this();
                resolver_.async_resolve(option_.server, std::to_string(option_.port),
                    [this, self](std::error_code ec, asio::ip::tcp::resolver::results_type results)
                    {
                        if (!ec)
                        {
                            connect_proxy(results);
                        }
                        else
                        {
                            LOG_ERROR("VMess resolve error: %s", ec.message().c_str());
                            handler_(ec, nullptr);
                        }
                    });
            }

            // 2. 连接到代理服务器
            // 尝试连接解析出的 IP 地址列表
            void connect_proxy(const asio::ip::tcp::resolver::results_type& endpoints)
            {
                auto self = shared_from_this();
                asio::async_connect(socket_, endpoints,
                    [this, self](std::error_code ec, asio::ip::tcp::endpoint /*endpoint*/)
                    {
                        if (!ec)
                        {
                            handshake();
                        }
                        else
                        {
                            LOG_ERROR("VMess connect error: %s", ec.message().c_str());
                            handler_(ec, nullptr);
                        }
                    });
            }

            // 3. 握手 (VMess 协议握手)
            // TODO: 实现完整的 VMess 协议
            void handshake()
            {
                LOG_DEBUG("VMess connected to server %s:%d", option_.server.c_str(), option_.port);
                LOG_WARN("VMess protocol not implemented yet, acting as plain TCP tunnel (will likely fail)");

                // VMess 协议非常复杂，涉及：
                // 1. 认证 (UUID, Timestamp)
                // 2. 指令 (Command, Option, Security, Reserve, Address)
                // 3. 数据加密 (AES-128-CFB, AES-128-GCM, Chacha20-Poly1305)
                // 4. 传输层 (TCP, WebSocket, HTTP/2, gRPC, QUIC)

                // 目前仅建立 TCP 连接，后续需要实现完整的 VMess 协议栈
                // 这意味着目前无法真正使用 VMess 节点

                auto conn = std::make_unique<common::TcpConnection>(std::move(socket_));
                handler_(std::error_code(), std::move(conn));
            }

            VmessAdapter::Option option_;
            constant::Metadata metadata_;
            asio::ip::tcp::socket socket_;
            ProxyAdapter::ConnectHandler handler_;
            asio::ip::tcp::resolver resolver_;
        };

        // 构造函数
        VmessAdapter::VmessAdapter(Option option)
            : option_(std::move(option))
        {
        }

        std::string VmessAdapter::name() const
        {
            return option_.name;
        }

        constant::AdapterType VmessAdapter::type() const
        {
            return constant::AdapterType::Vmess;
        }

        // 发起连接
        // 创建 VmessDialer 并启动连接流程
        void VmessAdapter::dial(const constant::Metadata& metadata, asio::io_context& io_context, ConnectHandler handler)
        {
            std::make_shared<VmessDialer>(option_, metadata, io_context, std::move(handler))->start();
        }

    } // namespace adapter
} // namespace clash
