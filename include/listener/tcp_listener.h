#pragma once

#include "listener/listener_interface.h"
#include "log/log.h"
#include "tunnel/tunnel.h"
#include <asio.hpp>
#include <memory>
#include <string>

namespace clash {
namespace listener {

class TcpListener : public Listener, public std::enable_shared_from_this<TcpListener> {
public:
    TcpListener(asio::io_context& io_context, int port, std::shared_ptr<tunnel::Tunnel> tunnel);
    
    void start();
    void close() override;
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
