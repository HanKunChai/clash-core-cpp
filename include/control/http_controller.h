#pragma once

#include <string>
#include <memory>
#include <asio.hpp>
#include "tunnel/tunnel.h"

namespace clash
{
    namespace control
    {
        /**
         * @brief HTTP 外部控制器
         * 
         * 提供 RESTful API 接口，用于查询状态、切换代理、更新配置等。
         * 兼容 Clash API。
         */
        class HttpController
        {
        public:
            /**
             * @brief 构造函数
             * 
             * @param io_context IO 上下文
             * @param address 监听地址
             * @param port 监听端口
             * @param tunnel 隧道管理器引用
             */
            HttpController(asio::io_context& io_context, std::string address, int port, std::shared_ptr<tunnel::Tunnel> tunnel);

            /**
             * @brief 启动控制器
             */
            void start();

        private:
            /**
             * @brief 接受连接
             */
            void do_accept();

            asio::io_context& io_context_;
            asio::ip::tcp::acceptor acceptor_;
            std::shared_ptr<tunnel::Tunnel> tunnel_;
        };

    } // namespace control
} // namespace clash
