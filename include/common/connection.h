#pragma once

#include <asio.hpp>
#include <functional>
#include <memory>

namespace clash
{
    namespace common
    {
        /**
         * @brief 连接接口
         * 
         * 定义了通用的连接操作，如读写和关闭。
         * 抽象了底层传输细节（如 TCP, TLS 等）。
         */
        class Connection
        {
        public:
            virtual ~Connection() = default;
            
            using executor_type = asio::any_io_executor;
            using ReadHandler = std::function<void(std::error_code, std::size_t)>;
            using WriteHandler = std::function<void(std::error_code, std::size_t)>;

            /**
             * @brief 异步读取数据
             * 
             * @param buffer 接收缓冲区
             * @param handler 读取完成回调
             */
            virtual void async_read_some(const asio::mutable_buffer& buffer, ReadHandler handler) = 0;

            /**
             * @brief 异步写入数据
             * 
             * @param buffer 发送缓冲区
             * @param handler 写入完成回调
             */
            virtual void async_write(const asio::const_buffer& buffer, WriteHandler handler) = 0;

            /**
             * @brief 关闭连接
             */
            virtual void close() = 0;

            /**
             * @brief 获取执行器
             * 
             * @return asio::any_io_executor 执行器
             */
            virtual asio::any_io_executor get_executor() = 0;
        };

        /**
         * @brief TCP 连接实现
         * 
         * 基于 asio::ip::tcp::socket 实现 Connection 接口。
         */
        class TcpConnection : public Connection
        {
        public:
            /**
             * @brief 构造函数
             * 
             * @param socket TCP 套接字
             */
            TcpConnection(asio::ip::tcp::socket socket) : socket_(std::move(socket)) {}

            void async_read_some(const asio::mutable_buffer& buffer, ReadHandler handler) override
            {
                socket_.async_read_some(buffer, handler);
            }

            void async_write(const asio::const_buffer& buffer, WriteHandler handler) override
            {
                asio::async_write(socket_, buffer, handler);
            }

            void close() override
            {
                std::error_code ec;
                socket_.close(ec);
            }

            asio::any_io_executor get_executor() override
            {
                return socket_.get_executor();
            }

            /**
             * @brief 获取底层 socket
             * 
             * @return asio::ip::tcp::socket& 底层 socket 引用
             */
            asio::ip::tcp::socket& socket() { return socket_; }

        private:
            asio::ip::tcp::socket socket_;
        };

    } // namespace common
} // namespace clash
