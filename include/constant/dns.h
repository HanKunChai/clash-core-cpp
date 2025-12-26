#pragma once

#include <string>
#include <map>

namespace clash
{
    namespace constant
    {
        /**
         * @brief DNS 增强模式枚举
         */
        enum class DNSMode
        {
            DNSNormal,  // 普通模式：直接返回解析结果
            DNSFakeIP,  // FakeIP 模式：返回虚假 IP，后续通过映射还原域名
            DNSMapping  // 映射模式 (Redir-Host)：返回真实 IP，但记录域名映射
        };

    } // namespace constant
} // namespace clash
