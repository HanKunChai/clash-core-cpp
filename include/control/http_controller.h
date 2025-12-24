#pragma once

#include <string>
#include <memory>
#include <asio.hpp>
#include "tunnel/tunnel.h"

namespace clash {
namespace control {

class HttpController {
public:
    HttpController(asio::io_context& io_context, std::string address, int port, std::shared_ptr<tunnel::Tunnel> tunnel);
    void start();

private:
    void do_accept();

    asio::io_context& io_context_;
    asio::ip::tcp::acceptor acceptor_;
    std::shared_ptr<tunnel::Tunnel> tunnel_;
};

} // namespace control
} // namespace clash
