#pragma once

#include <atomic>
#include <mutex>

namespace clash
{
    namespace common
    {
        /**
         * @brief 全局流量管理器
         * 
         * 统计全局的上传和下载流量。
         */
        class TrafficManager
        {
        public:
            /**
             * @brief 获取单例实例
             * 
             * @return TrafficManager& 单例引用
             */
            static TrafficManager& instance();

            /**
             * @brief 增加上传流量
             * 
             * @param bytes 字节数
             */
            void addUpload(uint64_t bytes);

            /**
             * @brief 增加下载流量
             * 
             * @param bytes 字节数
             */
            void addDownload(uint64_t bytes);

            /**
             * @brief 获取总上传流量
             * 
             * @return uint64_t 总上传字节数
             */
            uint64_t totalUpload() const;

            /**
             * @brief 获取总下载流量
             * 
             * @return uint64_t 总下载字节数
             */
            uint64_t totalDownload() const;

            /**
             * @brief 重置计数器
             */
            void reset();

        private:
            TrafficManager() = default;
            ~TrafficManager() = default;
            TrafficManager(const TrafficManager&) = delete;
            TrafficManager& operator=(const TrafficManager&) = delete;

            std::atomic<uint64_t> upload_{0};
            std::atomic<uint64_t> download_{0};
        };

    } // namespace common
} // namespace clash
