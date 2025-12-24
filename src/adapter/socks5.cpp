#include "adapter/socks5.h"
#include "log/log.h"
#include "common/connection.h"
#include <asio.hpp>
#include <vector>
#include <memory>

namespace clash
{
namespace adapter
{

// Socks5Dialer: 辅助类，用于处理异步的 Socks5 握手过程
// 继承 enable_shared_from_this 以便在异步回调中保持自身存活
class Socks5Dialer : public std::enable_shared_from_this<Socks5Dialer>
{
public:
    Socks5Dialer(const Socks5Adapter::Option& option, const constant::Metadata& metadata, asio::io_context& io_context, ProxyAdapter::ConnectHandler handler)
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
    // 使用异步 DNS 解析器解析 SOCKS5 代理服务器的域名
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
                    log::error("Socks5 resolve error: {}", ec.message());
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
                    log::error("Socks5 connect error: {}", ec.message());
                    handler_(ec, nullptr);
                }
            });
    }

    // 3. 发送握手请求 (Version + Methods)
    // 告诉代理服务器我们支持的认证方法
    void handshake()
    {
        auto self = shared_from_this();
        // 构造握手包：
        // VER: 0x05 (SOCKS5)
        // NMETHODS: 0x01 (1种方法)
        // METHODS: 0x00 (NO AUTHENTICATION REQUIRED)
        // TODO: 支持用户名/密码认证 (0x02)
        buffer_[0] = 0x05;
        buffer_[1] = 0x01;
        buffer_[2] = 0x00;

        asio::async_write(socket_, asio::buffer(buffer_, 3),
            [this, self](std::error_code ec, std::size_t /*length*/)
            {
                if (!ec)
                {
                    read_handshake_response();
                }
                else
                {
                    handler_(ec, nullptr);
                }
            });
    }

    // 4. 读取握手响应
    // 检查代理服务器选中的认证方法
    void read_handshake_response()
    {
        auto self = shared_from_this();
        asio::async_read(socket_, asio::buffer(buffer_, 2),
            [this, self](std::error_code ec, std::size_t /*length*/)
            {
                if (!ec)
                {
                    // 验证版本和选中的方法
                    // 必须是 SOCKS5 且方法为 0x00 (No Auth)
                    if (buffer_[0] != 0x05 || buffer_[1] != 0x00)
                    {
                        log::error("Socks5 handshake failed: ver={}, method={}", (int)buffer_[0], (int)buffer_[1]);
                        handler_(std::make_error_code(std::errc::connection_refused), nullptr);
                        return;
                    }
                    send_request();
                }
                else
                {
                    handler_(ec, nullptr);
                }
            });
    }

    // 5. 发送连接请求 (CONNECT Target)
    // 告诉代理服务器我们要连接的目标地址
    void send_request()
    {
        auto self = shared_from_this();

        std::vector<uint8_t> req;
        req.push_back(0x05); // VER
        req.push_back(0x01); // CMD: CONNECT
        req.push_back(0x00); // RSV

        // 目标地址类型
        if (!metadata_.host.empty())
        {
            req.push_back(0x03); // ATYP: Domain
            req.push_back(static_cast<uint8_t>(metadata_.host.size())); // Domain Length
            req.insert(req.end(), metadata_.host.begin(), metadata_.host.end()); // Domain
        }
        else
        {
            // 如果 Host 为空，回退到使用 IP
            // 这里简单地将 IP 字符串作为 Domain 类型发送 (ATYP=0x03)
            // SOCKS5 协议允许这样做，代理服务器会识别并处理
            req.push_back(0x03);
            req.push_back(static_cast<uint8_t>(metadata_.dstIP.size()));
            req.insert(req.end(), metadata_.dstIP.begin(), metadata_.dstIP.end());
        }

        // 端口 (大端序)
        req.push_back((metadata_.dstPort >> 8) & 0xFF);
        req.push_back(metadata_.dstPort & 0xFF);

        // 使用 shared_ptr 管理请求 buffer，确保在异步写入期间有效
        auto req_buf = std::make_shared<std::vector<uint8_t>>(std::move(req));

        asio::async_write(socket_, asio::buffer(*req_buf),
            [this, self, req_buf](std::error_code ec, std::size_t /*length*/)
            {
                if (!ec)
                {
                    read_request_response();
                }
                else
                {
                    handler_(ec, nullptr);
                }
            });
    }

    // 6. 读取连接响应头部 (前 4 字节)
    // VER, REP, RSV, ATYP
    void read_request_response()
    {
        auto self = shared_from_this();
        asio::async_read(socket_, asio::buffer(buffer_, 4),
            [this, self](std::error_code ec, std::size_t /*length*/)
            {
                if (!ec)
                {
                    // 检查响应码 (REP)
                    // 0x00 表示成功
                    if (buffer_[1] != 0x00)
                    {
                        log::error("Socks5 request failed with code: {}", (int)buffer_[1]);
                        handler_(std::make_error_code(std::errc::connection_refused), nullptr);
                        return;
                    }

                    // 根据地址类型 (ATYP) 决定后续读取长度
                    int addr_type = buffer_[3];
                    if (addr_type == 0x01)
                    { // IPv4 (4 bytes) + Port (2 bytes)
                         consume_remaining(4 + 2);
                    }
                    else if (addr_type == 0x04)
                    { // IPv6 (16 bytes) + Port (2 bytes)
                         consume_remaining(16 + 2);
                    }
                    else if (addr_type == 0x03)
                    { // Domain (Variable length)
                        read_domain_length();
                    }
                    else
                    {
                        handler_(std::make_error_code(std::errc::address_family_not_supported), nullptr);
                    }
                }
                else
                {
                    handler_(ec, nullptr);
                }
            });
    }

    // 6.1 读取域名长度 (如果是 Domain 类型)
    // 先读取 1 字节长度，再读取域名内容和端口
    void read_domain_length()
    {
        auto self = shared_from_this();
        asio::async_read(socket_, asio::buffer(buffer_, 1),
             [this, self](std::error_code ec, std::size_t /*length*/)
             {
                 if (!ec)
                 {
                     int len = buffer_[0];
                     consume_remaining(len + 2); // Domain + Port
                 }
                 else
                 {
                     handler_(ec, nullptr);
                 }
             });
    }

    // 7. 读取剩余的响应数据 (BND.ADDR + BND.PORT) 并完成连接
    // 我们通常不关心 BND.ADDR 和 BND.PORT，但必须把它们从 socket 缓冲区中读走
    void consume_remaining(int length)
    {
        auto self = shared_from_this();
        auto buf = std::make_shared<std::vector<char>>(length);
        asio::async_read(socket_, asio::buffer(*buf),
            [this, self, buf](std::error_code ec, std::size_t /*length*/)
            {
                if (!ec)
                {
                    // 握手成功，将 Socket 移交给 Connection 对象
                    // Connection 对象将负责后续的数据转发
                    handler_(ec, std::make_unique<common::TcpConnection>(std::move(socket_)));
                }
                else
                {
                    handler_(ec, nullptr);
                }
            });
    }

    Socks5Adapter::Option option_;
    constant::Metadata metadata_;
    asio::ip::tcp::socket socket_;
    asio::ip::tcp::resolver resolver_;
    ProxyAdapter::ConnectHandler handler_;
    std::array<uint8_t, 1024> buffer_;
};

Socks5Adapter::Socks5Adapter(Option option) : option_(std::move(option))
{
}

std::string Socks5Adapter::name() const
{
    return option_.name;
}

constant::AdapterType Socks5Adapter::type() const
{
    return constant::AdapterType::Socks5;
}

void Socks5Adapter::dial(const constant::Metadata& metadata, asio::io_context& io_context, ConnectHandler handler)
{
    std::make_shared<Socks5Dialer>(option_, metadata, io_context, std::move(handler))->start();
}

} // namespace adapter
} // namespace clash
