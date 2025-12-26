#pragma once

#include <string>
#include "constant/rule.h"
#include "constant/metadata.h"

namespace clash
{
    namespace rule
    {
        /**
         * @brief 规则接口
         * 
         * 所有规则类都必须实现此接口。
         */
        class Rule
        {
        public:
            virtual ~Rule() = default;

            /**
             * @brief 获取规则类型
             * 
             * @return constant::RuleType 规则类型
             */
            virtual constant::RuleType type() const = 0;

            /**
             * @brief 检查元数据是否匹配规则
             * 
             * @param metadata 连接元数据
             * @return true 匹配
             * @return false 不匹配
             */
            virtual bool match(const constant::Metadata& metadata) const = 0;

            /**
             * @brief 获取适配器名称
             * 
             * @return std::string 适配器名称
             */
            virtual std::string adapter() const = 0;

            /**
             * @brief 获取规则载荷
             * 
             * @return std::string 规则载荷
             */
            virtual std::string payload() const = 0;

            /**
             * @brief 是否应该解析 IP
             * 
             * 某些规则 (如 IPCIDR) 可能需要解析域名为 IP 才能进行匹配。
             * 
             * @return true 应该解析
             * @return false 不应该解析
             */
            virtual bool shouldResolveIP() const
            {
                return false;
            }
        };

    } // namespace rule
} // namespace clash
