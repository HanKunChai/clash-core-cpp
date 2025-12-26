#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <asio.hpp>

namespace clash
{
    namespace dns
    {
        /**
         * @brief DNS 解析器
         * 
         * 支持系统 DNS、自定义 UDP DNS 服务器和 Hosts 静态映射。
         */
        class Resolver : public std::enable_shared_from_this<Resolver>
        {
        public:
            using ResolveHandler = std::function<void(std::error_code, std::vector<asio::ip::address>)>;

            /**
             * @brief 构造函数
             * 
             * @param io_context IO 上下文
             */
            Resolver(asio::io_context& io_context);

            /**
             * @brief 异步解析域名
             * 
             * @param hostname 域名
             * @param handler 解析回调
             */
            void resolve(const std::string& hostname, ResolveHandler handler);

            /**
             * @brief 设置上游 DNS 服务器
             * 
             * @param nameservers DNS 服务器列表 (IP:Port)
             */
            void setNameServers(const std::vector<std::string>& nameservers);

            /**
             * @brief 设置 Hosts
             * 
             * @param hosts 静态映射表
             */
            void setHosts(const std::map<std::string, std::string>& hosts);

        private:
            asio::io_context& io_context_;
            asio::ip::tcp::resolver resolver_;
            std::map<std::string, std::string> hosts_;
            std::vector<std::string> nameservers_;

            /**
             * @brief 使用 UDP 协议解析
             * 
             * @param hostname 域名
             * @param nameserver DNS 服务器地址
             * @param handler 回调
             */
            void resolve_udp(const std::string& hostname, const std::string& nameserver, ResolveHandler handler);
        };

    } // namespace dns
} // namespace clash
