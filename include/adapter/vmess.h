#pragma once

#include "adapter/proxy_adapter.h"
#include <string>
#include <memory>
#include <asio.hpp>

namespace clash
{
namespace adapter
{

// VmessAdapter: VMess 协议适配器
// V2Ray 的核心协议
class VmessAdapter : public ProxyAdapter, public std::enable_shared_from_this<VmessAdapter>
{
public:
    // VMess 配置选项
    struct Option
    {
        std::string name;
        std::string server;
        int port;
        std::string uuid;
        int alterId = 0;
        std::string cipher;
        bool udp = false;
        bool tls = false;
        std::string network = "tcp"; // "tcp", "ws", "h2", "grpc"
        std::string wsPath;
        std::string wsHeaders;
    };

    // 构造函数
    VmessAdapter(Option option);

    std::string name() const override;
    constant::AdapterType type() const override;

    // 发起连接
    void dial(const constant::Metadata& metadata, asio::io_context& io_context, ConnectHandler handler) override;

private:
    Option option_;
};

} // namespace adapter
} // namespace clash

