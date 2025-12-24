#include "listener/tcp_listener.h"
#include "listener/session.h"
#include "log/log.h"

namespace clash {
namespace listener {

// 构造函数
// 初始化 TCP 监听器，绑定到指定端口 (IPv4)
// 持有 Tunnel 引用，用于后续创建 Session 时传递
TcpListener::TcpListener(asio::io_context& io_context, int port, std::shared_ptr<tunnel::Tunnel> tunnel)
    : io_context_(io_context),
      acceptor_(io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)),
      port_(port),
      tunnel_(std::move(tunnel)) {
}

// 启动监听
// 打印日志并开始异步接受连接
void TcpListener::start() {
    log::info("TCP Listener started on port {}", port_);
    do_accept();
}

// 关闭监听
// 关闭 Acceptor，停止接受新连接
void TcpListener::close() {
    acceptor_.close();
    log::info("TCP Listener on port {} closed", port_);
}

// 获取监听地址
// 返回 "IP:Port" 格式的字符串
std::string TcpListener::address() const {
    try {
        return acceptor_.local_endpoint().address().to_string() + ":" + std::to_string(acceptor_.local_endpoint().port());
    } catch (...) {
        return "0.0.0.0:" + std::to_string(port_);
    }
}

// 核心接受循环
// 异步等待新的 TCP 连接
// 1. 接受连接成功后，创建 Session 对象
// 2. 启动 Session 处理后续数据
// 3. 递归调用 do_accept 继续等待下一个连接
void TcpListener::do_accept() {
    acceptor_.async_accept(
        [this](std::error_code ec, asio::ip::tcp::socket socket) {
            if (!ec) {
                try {
                    log::debug("New connection from {}", socket.remote_endpoint().address().to_string());
                    // 创建并启动 Session
                    // Session 使用 shared_from_this 管理自身生命周期，这里只需启动即可
                    std::make_shared<Session>(std::move(socket), tunnel_)->start();
                } catch (const std::exception& e) {
                    log::error("Error creating session: {}", e.what());
                }
            } else {
                // 如果 Acceptor 被关闭 (ec == operation_aborted)，则不再继续
                if (ec != asio::error::operation_aborted) {
                    log::error("Accept error: {}", ec.message());
                }
            }

            // 只要 Acceptor 还是打开的，就继续接受新连接
            if (acceptor_.is_open()) {
                do_accept();
            }
        });
}

} // namespace listener
} // namespace clash
