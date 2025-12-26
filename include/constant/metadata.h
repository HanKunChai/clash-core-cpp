#pragma once

#include <string>
#include <optional>
#include "constant/adapters.h"

namespace clash
{
    namespace constant
    {
        /**
         * @brief 连接元数据
         * 
         * 包含连接的源地址、目标地址、协议类型等信息。
         * 用于路由匹配和日志记录。
         */
        struct Metadata
        {
            /**
             * @brief 连接类型枚举
             */
            enum class Type
            {
                Socks5, // SOCKS5 代理连接
                Http,   // HTTP 代理连接
                Redir,  // 透明代理 (Redirect) 连接
                Tproxy  // 透明代理 (TProxy) 连接
            };

            Type type = Type::Socks5; // 连接类型
            std::string srcIP;        // 源 IP 地址
            int srcPort = 0;          // 源端口
            std::string dstIP;        // 目标 IP 地址
            int dstPort = 0;          // 目标端口
            std::string host;         // 目标域名 (如果存在)
            
            /**
             * @brief 获取有效的目标地址
             * 
             * 优先返回域名，如果域名为空则返回 IP 地址。
             * 
             * @return std::string 目标地址
             */
            std::string destination() const
            {
                if (!host.empty()) return host;
                return dstIP;
            }
        };

    } // namespace constant
} // namespace clash
