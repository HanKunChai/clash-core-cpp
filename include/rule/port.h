#pragma once

#include "rule/rule_interface.h"
#include <string>

namespace clash
{
    namespace rule
    {
        /**
         * @brief 端口匹配规则
         * 
         * 匹配源端口、目的端口或入站端口。
         */
        class Port : public Rule
        {
        public:
            /**
             * @brief 端口规则类型
             */
            enum class Type
            {
                SrcPort,     ///< 源端口
                DstPort,     ///< 目的端口
                InboundPort  ///< 入站端口
            };

            /**
             * @brief 构造函数
             * 
             * @param type 端口规则类型
             * @param port 端口号
             * @param adapter 目标适配器名称
             */
            Port(Type type, int port, std::string adapter)
                : type_(type), port_(port), adapter_(std::move(adapter))
            {
            }

            /**
             * @brief 获取规则类型
             * 
             * @return constant::RuleType 规则类型
             */
            constant::RuleType type() const override
            {
                switch (type_)
                {
                    case Type::SrcPort:
                        return constant::RuleType::SrcPort;
                    case Type::DstPort:
                        return constant::RuleType::DstPort;
                    case Type::InboundPort:
                        return constant::RuleType::InboundPort;
                }
                return constant::RuleType::DstPort;
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
                switch (type_)
                {
                    case Type::SrcPort:
                        return metadata.srcPort == port_;
                    case Type::DstPort:
                        return metadata.dstPort == port_;
                    // InboundPort not fully supported in metadata yet, assuming 0 or need update
                    case Type::InboundPort:
                        return false; // TODO: Add inbound port to metadata
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
             * @brief 获取规则载荷 (端口号)
             * 
             * @return std::string 规则载荷
             */
            std::string payload() const override
            {
                return std::to_string(port_);
            }

        private:
            Type type_;
            int port_;
            std::string adapter_;
        };

    } // namespace rule
} // namespace clash
