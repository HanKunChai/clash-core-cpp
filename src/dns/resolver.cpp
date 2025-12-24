#include "dns/resolver.h"
#include "dns/message.h"
#include "log/log.h"
#include <random>

namespace clash
{
namespace dns
{

// 构造函数
// 初始化 IO Context 和系统默认解析器
Resolver::Resolver(asio::io_context& io_context)
    : io_context_(io_context), resolver_(io_context)
{
}

// 异步解析域名
// 解析顺序：
// 1. 检查 Hosts 配置
// 2. 如果配置了自定义 Nameserver，使用 UDP 发起 DNS 查询
// 3. 否则使用系统默认解析器 (System Resolver)
void Resolver::resolve(const std::string& hostname, ResolveHandler handler)
{
    // 1. Check Hosts
    auto it = hosts_.find(hostname);
    if (it != hosts_.end())
    {
        std::error_code ec;
        auto addr = asio::ip::make_address(it->second, ec);
        if (!ec)
        {
            // 如果在 Hosts 中找到，直接通过 post 回调结果
            asio::post(io_context_, [handler, addr]()
            {
                handler(std::error_code(), {addr});
            });
            return;
        }
    }

    // 2. Custom DNS (UDP)
    if (!nameservers_.empty())
    {
        // 使用第一个 Nameserver 进行查询
        // TODO: 实现并发查询或故障转移 (Fallback)
        resolve_udp(hostname, nameservers_[0], handler);
        return;
    }

    // 3. System Resolve (Fallback)
    // 使用 asio::ip::tcp::resolver 进行系统级解析
    resolver_.async_resolve(hostname, "",
        [handler](const std::error_code& ec, asio::ip::tcp::resolver::results_type results)
        {
            std::vector<asio::ip::address> addresses;
            if (!ec)
            {
                for (const auto& entry : results)
                {
                    addresses.push_back(entry.endpoint().address());
                }
            }
            handler(ec, addresses);
        });
}

// 设置上游 DNS 服务器
void Resolver::setNameServers(const std::vector<std::string>& nameservers)
{
    nameservers_ = nameservers;
    if (!nameservers_.empty())
    {
        log::info("Using custom DNS server: {}", nameservers_[0]);
    }
}

// 设置 Hosts 映射
void Resolver::setHosts(const std::map<std::string, std::string>& hosts)
{
    hosts_ = hosts;
}

// UDP 解析实现
// 手动构造 DNS 查询包并通过 UDP 发送
void Resolver::resolve_udp(const std::string& hostname, const std::string& nameserver, ResolveHandler handler)
{
    auto socket = std::make_shared<asio::ip::udp::socket>(io_context_);
    
    // 解析 Nameserver IP
    std::error_code ec;
    auto ns_addr = asio::ip::make_address(nameserver, ec);
    if (ec)
    {
        log::error("Invalid nameserver IP: {}", nameserver);
        handler(ec, {});
        return;
    }
    asio::ip::udp::endpoint ns_endpoint(ns_addr, 53);

    socket->open(ns_endpoint.protocol());

    // 构造 DNS 查询消息
    Message msg;
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint16_t> dis(0, 65535);
    
    msg.header.id = dis(gen); // 随机 Transaction ID
    msg.header.flags = 0x0100; // Standard query, Recursion Desired
    msg.addQuestion(hostname, 1); // Type A (IPv4)
    // TODO: 同时查询 AAAA (IPv6)

    auto buffer = std::make_shared<std::vector<uint8_t>>(msg.encode());
    auto recv_buffer = std::make_shared<std::vector<uint8_t>>(512); // 标准 DNS 包大小限制

    auto self = shared_from_this();

    // 发送查询
    socket->async_send_to(asio::buffer(*buffer), ns_endpoint,
        [this, self, socket, recv_buffer, handler](std::error_code ec, std::size_t /*bytes_sent*/)
        {
            if (ec)
            {
                handler(ec, {});
                return;
            }

            // 接收响应
            socket->async_receive_from(asio::buffer(*recv_buffer), *std::make_shared<asio::ip::udp::endpoint>(),
                [this, self, socket, recv_buffer, handler](std::error_code ec, std::size_t bytes_recvd)
                {
                    if (ec)
                    {
                        handler(ec, {});
                        return;
                    }

                    recv_buffer->resize(bytes_recvd);
                    Message response;
                    // 解码 DNS 响应
                    if (Message::decode(*recv_buffer, response))
                    {
                        std::vector<asio::ip::address> addresses;
                        for (const auto& ans : response.answers)
                        {
                            if (ans.type == 1 && ans.rdlength == 4) // A record
                            { 
                                asio::ip::address_v4::bytes_type bytes;
                                std::copy_n(ans.rdata.begin(), 4, bytes.begin());
                                addresses.push_back(asio::ip::address_v4(bytes));
                            }
                            // TODO: 处理 AAAA 记录
                        }
                        handler(std::error_code(), addresses);
                    }
                    else
                    {
                        handler(std::make_error_code(std::errc::bad_message), {});
                    }
                });
        });
}

} // namespace dns
} // namespace clash

