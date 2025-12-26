#pragma once

#include <string>

namespace clash
{
    namespace constant
    {
        /**
         * @brief 路由规则类型枚举
         */
        enum class RuleType
        {
            Domain,         // 域名完全匹配
            DomainSuffix,   // 域名后缀匹配
            DomainKeyword,  // 域名关键字匹配
            GEOIP,          // GeoIP 国家/地区匹配
            IPCIDR,         // 目标 IP CIDR 匹配
            SrcIPCIDR,      // 源 IP CIDR 匹配
            SrcPort,        // 源端口匹配
            DstPort,        // 目标端口匹配
            InboundPort,    // 入站端口匹配
            Process,        // 进程名称匹配 (全名)
            ProcessPath,    // 进程路径匹配
            IPSet,          // IPSet 匹配 (Linux)
            MATCH           // 全匹配 (默认规则)
        };

    } // namespace constant
} // namespace clash
