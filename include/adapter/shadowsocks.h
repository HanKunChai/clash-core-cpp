#pragma once

#include "adapter/proxy_adapter.h"
#include "dns/resolver.h"
#include <string>
#include <memory>
#include <asio.hpp>

namespace clash
{
namespace adapter
{

// ShadowsocksAdapter: Shadowsocks 协议适配器
class ShadowsocksAdapter : public ProxyAdapter, public std::enable_shared_from_this<ShadowsocksAdapter>
{
public:
    // Shadowsocks 配置选项
    struct Option
    {
        std::string name;
        std::string server;
        int port;
        std::string password;
        std::string cipher;
        bool udp = false;
    };

    // 构造函数
    ShadowsocksAdapter(Option option);

    std::string name() const override;
    constant::AdapterType type() const override;

    // 发起连接
    void dial(const constant::Metadata& metadata, asio::io_context& io_context, ConnectHandler handler) override;

private:
    Option option_;
};

} // namespace adapter
} // namespace clash

