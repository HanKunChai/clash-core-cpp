#pragma once

#include "common/trackable.h"
#include <memory>
#include <mutex>
#include <map>
#include <vector>

namespace clash
{
    namespace common
    {
        /**
         * @brief 连接管理器
         * 
         * 负责跟踪和管理所有活跃的连接。
         * 提供查询和强制关闭连接的功能。
         */
        class ConnectionManager
        {
        public:
            /**
             * @brief 获取单例实例
             * 
             * @return ConnectionManager& 单例引用
             */
            static ConnectionManager& instance();

            /**
             * @brief 添加连接
             * 
             * @param conn 可追踪的连接对象
             */
            void add(std::shared_ptr<Trackable> conn);

            /**
             * @brief 移除连接
             * 
             * @param id 连接 ID
             */
            void remove(const std::string& id);
            
            /**
             * @brief 获取所有连接
             * 
             * @return std::vector<std::shared_ptr<Trackable>> 连接列表
             */
            std::vector<std::shared_ptr<Trackable>> getAll() const;

            /**
             * @brief 关闭指定连接
             * 
             * @param id 连接 ID
             */
            void close(const std::string& id);

        private:
            ConnectionManager() = default;
            
            mutable std::mutex mutex_;
            std::map<std::string, std::weak_ptr<Trackable>> connections_;
        };

    } // namespace common
} // namespace clash
