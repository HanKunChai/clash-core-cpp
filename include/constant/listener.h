#pragma once

#include <string>

namespace clash
{
    namespace constant
    {
        /**
         * @brief 入站连接类型枚举
         */
        enum class InboundType
        {
            Socks,  // SOCKS5 代理
            Redir,  // 透明代理 (Redirect)
            Tproxy, // 透明代理 (TProxy)
            HTTP,   // HTTP 代理
            Mixed   // 混合端口 (HTTP + SOCKS5)
        };

        /**
         * @brief 入站配置结构
         */
        struct Inbound
        {
            InboundType type;           // 入站类型
            std::string bindAddress;    // 绑定地址
            bool isFromPortCfg = false; // 是否来自旧版端口配置
            int port = 0;               // 监听端口
        };

    } // namespace constant
} // namespace clash
