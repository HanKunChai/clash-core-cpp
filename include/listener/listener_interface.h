#pragma once

#include <string>

namespace clash
{
    namespace listener
    {
        /**
         * @brief 监听器接口
         * 
         * 定义了监听器的基本操作，如关闭和获取地址。
         */
        class Listener
        {
        public:
            virtual ~Listener() = default;
            
            /**
             * @brief 关闭监听器
             */
            virtual void close() = 0;

            /**
             * @brief 获取监听地址
             * 
             * @return std::string 监听地址 (IP:Port)
             */
            virtual std::string address() const = 0;
        };

    } // namespace listener
} // namespace clash
