#pragma once

#include <string>
#include <memory>
#include <functional>
#include "constant/adapters.h"
#include "constant/metadata.h"
#include "common/connection.h"
#include <asio.hpp>

namespace clash
{
    namespace adapter
    {
        /**
         * @brief 代理适配器基类
         * 
         * 所有代理类型（Shadowsocks, VMess, URLTest 等）都继承自此类。
         * 定义了代理的基本接口，如名称、类型和连接方法。
         */
        class ProxyAdapter
        {
        public:
            virtual ~ProxyAdapter() = default;
            
            /**
             * @brief 获取代理名称
             * 
             * @return std::string 代理名称
             */
            virtual std::string name() const = 0;
            
            /**
             * @brief 获取代理类型
             * 
             * @return constant::AdapterType 代理类型
             */
            virtual constant::AdapterType type() const = 0;
            
            /**
             * @brief 连接回调函数类型
             * 
             * @param std::error_code 错误码
             * @param std::unique_ptr<common::Connection> 建立的连接对象
             */
            using ConnectHandler = std::function<void(std::error_code, std::unique_ptr<common::Connection>)>;

            /**
             * @brief 发起连接
             * 
             * @param metadata 连接元数据（目标地址、端口等）
             * @param io_context IO 上下文
             * @param handler 连接完成回调
             */
            virtual void dial(const constant::Metadata& metadata, asio::io_context& io_context, ConnectHandler handler) = 0;
        };

    } // namespace adapter
} // namespace clash

