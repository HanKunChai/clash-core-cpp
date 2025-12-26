#pragma once

namespace clash
{
    namespace tunnel
    {
        /**
         * @brief 隧道模式
         */
        enum class Mode
        {
            Global, ///< 全局代理模式
            Rule,   ///< 规则模式
            Direct, ///< 直连模式
            Script  ///< 脚本模式
        };

    } // namespace tunnel
} // namespace clash
