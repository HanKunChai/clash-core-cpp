#pragma once

#include "rule/rule_interface.h"
#include <algorithm>
#include <string>

namespace clash
{
    namespace rule
    {
        /**
         * @brief 域名关键字匹配规则
         * 
         * 匹配包含特定关键字的域名。
         */
        class DomainKeyword : public Rule
        {
        public:
            /**
             * @brief 构造函数
             * 
             * @param keyword 关键字
             * @param adapter 目标适配器名称
             */
            DomainKeyword(std::string keyword, std::string adapter)
                : keyword_(std::move(keyword)), adapter_(std::move(adapter))
            {
                std::transform(keyword_.begin(), keyword_.end(), keyword_.begin(), ::tolower);
            }

            /**
             * @brief 获取规则类型
             * 
             * @return constant::RuleType 规则类型
             */
            constant::RuleType type() const override
            {
                return constant::RuleType::DomainKeyword;
            }

            /**
             * @brief 检查元数据是否匹配规则
             * 
             * @param metadata 连接元数据
             * @return true 匹配
             * @return false 不匹配
             */
            bool match(const constant::Metadata& metadata) const override
            {
                if (metadata.host.empty())
                {
                    return false;
                }
                std::string host = metadata.host;
                std::transform(host.begin(), host.end(), host.begin(), ::tolower);

                return host.find(keyword_) != std::string::npos;
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
             * @brief 获取规则载荷 (关键字)
             * 
             * @return std::string 规则载荷
             */
            std::string payload() const override
            {
                return keyword_;
            }

        private:
            std::string keyword_;
            std::string adapter_;
        };

    } // namespace rule
} // namespace clash
