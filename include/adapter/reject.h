#pragma once

#include "adapter/proxy_adapter.h"

namespace clash
{
namespace adapter
{

// RejectAdapter: 拒绝连接适配器
// 用于拦截流量，直接返回连接拒绝错误
class RejectAdapter : public ProxyAdapter
{
public:
    std::string name() const override
    {
        return "REJECT";
    }

    constant::AdapterType type() const override
    {
        return constant::AdapterType::Reject;
    }

    // 发起连接
    // 立即调用 handler 并返回 connection_refused 错误
    void dial(const constant::Metadata& /*metadata*/, asio::io_context& /*io_context*/, ConnectHandler handler) override
    {
        // 模拟连接被拒绝
        handler(std::make_error_code(std::errc::connection_refused), nullptr);
    }
};

} // namespace adapter
} // namespace clash

