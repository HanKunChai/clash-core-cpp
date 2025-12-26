#pragma once

#include "constant/metadata.h"
#include <string>
#include <chrono>

namespace clash
{
    namespace common
    {
        /**
         * @brief 可追踪对象接口
         * 
         * 定义了连接追踪所需的基本信息，如 ID、元数据、流量统计等。
         */
        class Trackable
        {
        public:
            virtual ~Trackable() = default;
            
            /**
             * @brief 获取连接 ID
             * 
             * @return std::string 连接 ID
             */
            virtual std::string id() const = 0;

            /**
             * @brief 获取连接元数据
             * 
             * @return constant::Metadata 元数据
             */
            virtual constant::Metadata metadata() const = 0;

            /**
             * @brief 获取上传流量
             * 
             * @return uint64_t 上传字节数
             */
            virtual uint64_t upload() const = 0;

            /**
             * @brief 获取下载流量
             * 
             * @return uint64_t 下载字节数
             */
            virtual uint64_t download() const = 0;

            /**
             * @brief 获取连接开始时间
             * 
             * @return std::chrono::system_clock::time_point 开始时间
             */
            virtual std::chrono::system_clock::time_point startTime() const = 0;

            /**
             * @brief 获取代理链
             * 
             * @return std::string 代理链描述
             */
            virtual std::string chain() const = 0;

            /**
             * @brief 关闭连接
             */
            virtual void close() = 0;
        };

    } // namespace common
} // namespace clash
