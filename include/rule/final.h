#pragma once

#include "rule/rule_interface.h"

namespace clash
{
    namespace rule
    {
        /**
         * @brief 最终匹配规则 (MATCH)
         * 
         * 匹配所有未被其他规则匹配的流量。
         */
        class Final : public Rule
        {
        public:
            /**
             * @brief 构造函数
             * 
             * @param adapter 目标适配器名称
             */
            Final(std::string adapter) : adapter_(std::move(adapter)) {}

            /**
             * @brief 获取规则类型
             * 
             * @return constant::RuleType 规则类型
             */
            constant::RuleType type() const override
            {
                return constant::RuleType::MATCH;
            }

            /**
             * @brief 检查元数据是否匹配规则
             * 
             * @param metadata 连接元数据
             * @return true 总是匹配
             */
            bool match(const constant::Metadata& /*metadata*/) const override
            {
                return true;
            }

            /**
             * @brief 获取适配器名称
             * 
             * @return std::string 适配器名称
             */
            std::string adapter() const override
            {
                return adapter_;
            }

            /**
             * @brief 获取规则载荷
             * 
             * @return std::string 空字符串
             */
            std::string payload() const override
            {
                return "";
            }

        private:
            std::string adapter_;
        };

    } // namespace rule
} // namespace clash
