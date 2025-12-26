#pragma once

#include "rule/rule_interface.h"
#include <string>

namespace clash
{
    namespace rule
    {
        /**
         * @brief 进程名称匹配规则
         * 
         * 匹配发起连接的进程名称。
         */
        class Process : public Rule
        {
        public:
            /**
             * @brief 构造函数
             * 
             * @param processName 进程名称
             * @param adapter 目标适配器名称
             */
            Process(std::string processName, std::string adapter);

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
             * @brief 获取规则载荷 (进程名称)
             * 
             * @return std::string 规则载荷
             */
            std::string payload() const override;

        private:
            std::string processName_;
            std::string adapter_;
        };

        /**
         * @brief 进程路径匹配规则
         * 
         * 匹配发起连接的进程路径。
         */
        class ProcessPath : public Rule
        {
        public:
            /**
             * @brief 构造函数
             * 
             * @param processPath 进程路径
             * @param adapter 目标适配器名称
             */
            ProcessPath(std::string processPath, std::string adapter);

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
             * @brief 获取规则载荷 (进程路径)
             * 
             * @return std::string 规则载荷
             */
            std::string payload() const override;

        private:
            std::string processPath_;
            std::string adapter_;
        };

    } // namespace rule
} // namespace clash
