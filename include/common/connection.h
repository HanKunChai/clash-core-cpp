#pragma once

#include <asio.hpp>
#include <functional>
#include <memory>

namespace clash {
namespace common {

class Connection {
public:
    virtual ~Connection() = default;
    
    using executor_type = asio::any_io_executor;
    using ReadHandler = std::function<void(std::error_code, std::size_t)>;
    using WriteHandler = std::function<void(std::error_code, std::size_t)>;

    virtual void async_read_some(const asio::mutable_buffer& buffer, ReadHandler handler) = 0;
    virtual void async_write(const asio::const_buffer& buffer, WriteHandler handler) = 0;
    virtual void close() = 0;
    virtual asio::any_io_executor get_executor() = 0;
};

class TcpConnection : public Connection {
public:
    TcpConnection(asio::ip::tcp::socket socket) : socket_(std::move(socket)) {}

    void async_read_some(const asio::mutable_buffer& buffer, ReadHandler handler) override {
        socket_.async_read_some(buffer, handler);
    }

    void async_write(const asio::const_buffer& buffer, WriteHandler handler) override {
        asio::async_write(socket_, buffer, handler);
    }

    void close() override {
        std::error_code ec;
        socket_.close(ec);
    }

    asio::any_io_executor get_executor() override {
        return socket_.get_executor();
    }

    asio::ip::tcp::socket& socket() { return socket_; }

private:
    asio::ip::tcp::socket socket_;
};

} // namespace common
} // namespace clash
