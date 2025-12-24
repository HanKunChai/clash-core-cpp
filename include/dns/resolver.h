#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <asio.hpp>

namespace clash {
namespace dns {

class Resolver : public std::enable_shared_from_this<Resolver> {
public:
    using ResolveHandler = std::function<void(std::error_code, std::vector<asio::ip::address>)>;

    Resolver(asio::io_context& io_context);

    // 异步解析域名
    void resolve(const std::string& hostname, ResolveHandler handler);

    // 设置上游 DNS 服务器
    void setNameServers(const std::vector<std::string>& nameservers);

    // 设置 Hosts
    void setHosts(const std::map<std::string, std::string>& hosts);

private:
    asio::io_context& io_context_;
    asio::ip::tcp::resolver resolver_;
    std::map<std::string, std::string> hosts_;
    std::vector<std::string> nameservers_;

    void resolve_udp(const std::string& hostname, const std::string& nameserver, ResolveHandler handler);
};

} // namespace dns
} // namespace clash
