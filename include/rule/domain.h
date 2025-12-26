#pragma once

#include "rule/rule_interface.h"
#include <algorithm>
#include <string>

namespace clash
{
    namespace rule
    {
        /**
         * @brief 域名匹配规则
         * 
         * 匹配完全相等的域名。
         */
        class Domain : public Rule
        {
        public:
            /**
             * @brief 构造函数
             * 
             * @param domain 目标域名
             * @param adapter 目标适配器名称
             */
            Domain(std::string domain, std::string adapter)
                : domain_(std::move(domain)), adapter_(std::move(adapter))
            {
                std::transform(domain_.begin(), domain_.end(), domain_.begin(), ::tolower);
            }

            /**
             * @brief 获取规则类型
             * 
             * @return constant::RuleType 规则类型
             */
            constant::RuleType type() const override
            {
                return constant::RuleType::Domain;
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
                // 简单的忽略大小写比较 (假设 metadata.host 已经是小写或者在这里转换)
                // 为了性能，应该确保 metadata.host 只转换一次。
                // 这里我们只做一个简单的检查。
                std::string host = metadata.host;
                std::transform(host.begin(), host.end(), host.begin(), ::tolower);
                return host == domain_;
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
             * @brief 获取规则载荷 (域名)
             * 
             * @return std::string 规则载荷
             */
            std::string payload() const override
            {
                return domain_;
            }

        private:
            std::string domain_;
            std::string adapter_;
        };

        /**
         * @brief 域名后缀匹配规则
         * 
         * 匹配域名后缀。
         */
        class DomainSuffix : public Rule
        {
        public:
            /**
             * @brief 构造函数
             * 
             * @param suffix 域名后缀
             * @param adapter 目标适配器名称
             */
            DomainSuffix(std::string suffix, std::string adapter)
                : suffix_(std::move(suffix)), adapter_(std::move(adapter))
            {
                std::transform(suffix_.begin(), suffix_.end(), suffix_.begin(), ::tolower);
            }

            /**
             * @brief 获取规则类型
             * 
             * @return constant::RuleType 规则类型
             */
            constant::RuleType type() const override
            {
                return constant::RuleType::DomainSuffix;
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

                if (host.length() < suffix_.length())
                {
                    return false;
                }
                
                // 精确匹配
                if (host == suffix_)
                {
                    return true;
                }
                
                // 后缀匹配 (必须以点号开头)
                if (host.length() > suffix_.length())
                {
                    if (host.compare(host.length() - suffix_.length(), suffix_.length(), suffix_) == 0)
                    {
                        return host[host.length() - suffix_.length() - 1] == '.';
                    }
                }
                return false;
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
             * @brief 获取规则载荷 (域名后缀)
             * 
             * @return std::string 规则载荷
             */
            std::string payload() const override
            {
                return suffix_;
            }

        private:
            std::string suffix_;
            std::string adapter_;
        };

    } // namespace rule
} // namespace clash
