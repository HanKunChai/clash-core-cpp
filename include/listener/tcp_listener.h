#pragma once

#include "listener/listener_interface.h"
#include "log/log.h"
#include "tunnel/tunnel.h"
#include <asio.hpp>
#include <memory>
#include <string>

namespace clash
{
    namespace listener
    {
        /**
         * @brief TCP 监听器
         * 
         * 监听指定端口的 TCP 连接，并为每个连接创建 Session。
         * 支持 HTTP 和 SOCKS5 混合端口。
         */
        class TcpListener : public Listener, public std::enable_shared_from_this<TcpListener>
        {
        public:
            /**
             * @brief 构造函数
             * 
             * @param io_context IO 上下文
             * @param port 监听端口
             * @param tunnel 隧道管理器
             */
            TcpListener(asio::io_context& io_context, int port, std::shared_ptr<tunnel::Tunnel> tunnel);
            
            /**
             * @brief 启动监听
             */
            void start();

            /**
             * @brief 关闭监听
             */
            void close() override;

            /**
             * @brief 获取监听地址
             * 
             * @return std::string 监听地址
             */
            std::string address() const override;

        private:
            void do_accept();

            asio::io_context& io_context_;
            asio::ip::tcp::acceptor acceptor_;
            int port_;
            std::shared_ptr<tunnel::Tunnel> tunnel_;
        };

    } // namespace listener
} // namespace clash
