#pragma once

#include <string>
#include <optional>

namespace clash
{
    namespace common
    {
        /**
         * @brief 平台相关工具类
         * 
         * 提供跨平台或特定平台的系统调用封装。
         */
        class Platform
        {
        public:
            /**
             * @brief 根据源端口获取进程路径
             * 
             * @param port 源端口
             * @return std::string 进程可执行文件路径
             */
            static std::string getProcessPath(int port);
        };

    } // namespace common
} // namespace clash
