#pragma once

#include "adapter/proxy_adapter.h"

namespace clash
{
    namespace adapter
    {
        /**
         * @brief 拒绝连接适配器
         * 
         * 用于拦截流量，直接返回连接拒绝错误。
         */
        class RejectAdapter : public ProxyAdapter
        {
        public:
            /**
             * @brief 获取代理名称
             * 
             * @return std::string 代理名称 "REJECT"
             */
            std::string name() const override
            {
                return "REJECT";
            }

            /**
             * @brief 获取代理类型
             * 
             * @return constant::AdapterType 代理类型 Reject
             */
            constant::AdapterType type() const override
            {
                return constant::AdapterType::Reject;
            }

            /**
             * @brief 发起连接
             * 
             * 立即调用 handler 并返回 connection_refused 错误。
             * 
             * @param metadata 连接元数据
             * @param io_context IO 上下文
             * @param handler 连接回调
             */
            void dial(const constant::Metadata& /*metadata*/, asio::io_context& /*io_context*/, ConnectHandler handler) override
            {
                // 模拟连接被拒绝
                handler(std::make_error_code(std::errc::connection_refused), nullptr);
            }
        };

    } // namespace adapter
} // namespace clash

