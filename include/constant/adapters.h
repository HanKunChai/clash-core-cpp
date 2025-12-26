#pragma once

#include <string>
#include <vector>

namespace clash
{
    namespace constant
    {
        /**
         * @brief 代理适配器类型枚举
         */
        enum class AdapterType
        {
            Direct,         // 直连
            Reject,         // 拒绝
            Shadowsocks,    // Shadowsocks
            ShadowsocksR,   // ShadowsocksR (SSR)
            Snell,          // Snell
            Socks5,         // SOCKS5
            Http,           // HTTP
            Vmess,          // VMess
            Trojan,         // Trojan
            Relay,          // 中继 (Relay)
            Selector,       // 手动选择 (Select)
            URLTest,        // 自动测速 (URL-Test)
            Fallback,       // 故障转移 (Fallback)
            LoadBalance,    // 负载均衡 (Load-Balance)
        };

        /**
         * @brief 代理链类型定义
         * 
         * 记录流量经过的代理节点名称列表。
         */
        using Chain = std::vector<std::string>;

    } // namespace constant
} // namespace clash
