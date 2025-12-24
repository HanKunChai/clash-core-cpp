#pragma once

#include "adapter/proxy_adapter.h"
#include <string>

namespace clash
{
namespace adapter
{

// Socks5Adapter: Socks5 协议适配器
// 实现标准的 Socks5 客户端协议
class Socks5Adapter : public ProxyAdapter, public std::enable_shared_from_this<Socks5Adapter>
{
public:
    // Socks5 配置选项
    struct Option
    {
        std::string name;
        std::string server;
        int port;
        std::string username;
        std::string password;
    };

    // 构造函数
    Socks5Adapter(Option option);

    // 获取代理名称
    std::string name() const override;
    
    // 获取代理类型
    constant::AdapterType type() const override;

    // 发起连接：连接到 Socks5 代理服务器，完成握手，并连接到目标主机
    void dial(const constant::Metadata& metadata, asio::io_context& io_context, ConnectHandler handler) override;

private:
    Option option_;
};

} // namespace adapter
} // namespace clash

