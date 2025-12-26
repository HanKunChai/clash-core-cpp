#pragma once

#include "rule/ip_cidr.h"

namespace clash
{
    namespace rule
    {
        /**
         * @brief 源 IP CIDR 匹配规则
         * 
         * 匹配源 IP 地址是否属于指定 CIDR 网段。
         */
        class SrcIPCIDR : public IPCIDR
        {
        public:
            /**
             * @brief 构造函数
             * 
             * @param cidr CIDR 字符串
             * @param adapter 目标适配器名称
             */
            SrcIPCIDR(std::string cidr, std::string adapter)
                : IPCIDR(std::move(cidr), std::move(adapter), true)
            {
            } // SrcIP never needs DNS resolution

            /**
             * @brief 获取规则类型
             * 
             * @return constant::RuleType 规则类型
             */
            constant::RuleType type() const override
            {
                return constant::RuleType::SrcIPCIDR;
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
                if (metadata.srcIP.empty())
                {
                    return false;
                }

                std::error_code ec;
                auto src_addr = asio::ip::make_address(metadata.srcIP, ec);
                if (ec)
                {
                    return false;
                }

                if (src_addr.is_v4() && network_address_.is_v4())
                {
                    return match_v4(src_addr.to_v4());
                }
                else if (src_addr.is_v6() && network_address_.is_v6())
                {
                    return match_v6(src_addr.to_v6());
                }

                return false;
            }
            
            /**
             * @brief 是否应该解析 IP
             * 
             * 源 IP 规则不需要解析目标域名。
             * 
             * @return true 应该解析
             * @return false 不应该解析
             */
            bool shouldResolveIP() const override
            {
                return false;
            }
        };

    } // namespace rule
} // namespace clash
