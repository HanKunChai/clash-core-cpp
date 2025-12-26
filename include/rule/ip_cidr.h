#pragma once

#include "rule/rule_interface.h"
#include <asio.hpp>
#include <string>

namespace clash
{
    namespace rule
    {
        /**
         * @brief IP CIDR 匹配规则
         * 
         * 匹配属于指定 CIDR 网段的 IP 地址。
         */
        class IPCIDR : public Rule
        {
        public:
            /**
             * @brief 构造函数
             * 
             * @param cidr CIDR 字符串
             * @param adapter 目标适配器名称
             * @param noResolve 是否不解析域名
             */
            IPCIDR(std::string cidr, std::string adapter, bool noResolve = false);

            /**
             * @brief 获取规则类型
             * 
             * @return constant::RuleType 规则类型
             */
            constant::RuleType type() const override;

            /**
             * @brief 检查元数据是否匹配规则
             * 
             * @param metadata 连接元数据
             * @return true 匹配
             * @return false 不匹配
             */
            bool match(const constant::Metadata& metadata) const override;

            /**
             * @brief 获取适配器名称
             * 
             * @return std::string 适配器名称
             */
            std::string adapter() const override;

            /**
             * @brief 获取规则载荷 (CIDR)
             * 
             * @return std::string 规则载荷
             */
            std::string payload() const override;

            /**
             * @brief 是否应该解析 IP
             * 
             * @return true 应该解析
             * @return false 不应该解析
             */
            bool shouldResolveIP() const override;

        protected:
            std::string cidr_;
            std::string adapter_;
            bool noResolve_;
            
            asio::ip::address network_address_;
            int prefix_length_;

            /**
             * @brief 匹配 IPv4 地址
             * 
             * @param addr IPv4 地址
             * @return true 匹配
             * @return false 不匹配
             */
            bool match_v4(const asio::ip::address_v4& addr) const;

            /**
             * @brief 匹配 IPv6 地址
             * 
             * @param addr IPv6 地址
             * @return true 匹配
             * @return false 不匹配
             */
            bool match_v6(const asio::ip::address_v6& addr) const;
        };

    } // namespace rule
} // namespace clash
