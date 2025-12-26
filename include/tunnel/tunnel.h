#pragma once

#include <map>
#include <vector>
#include <string>
#include <memory>
#include "adapter/proxy_adapter.h"
#include "rule/rule_interface.h"
#include "constant/metadata.h"
#include "tunnel/mode.h"

namespace clash
{
    namespace dns
    {
        class Resolver;
    }

    namespace tunnel
    {
        /**
         * @brief 隧道管理器
         * 
         * 负责管理代理适配器、规则和模式，并根据规则匹配流量。
         */
        class Tunnel
        {
        public:
            /**
             * @brief 构造函数
             */
            Tunnel();
            
            /**
             * @brief 添加代理适配器
             * 
             * @param proxy 代理适配器指针
             */
            void addProxy(std::shared_ptr<adapter::ProxyAdapter> proxy);

            /**
             * @brief 获取指定名称的代理适配器
             * 
             * @param name 代理适配器名称
             * @return std::shared_ptr<adapter::ProxyAdapter> 代理适配器指针
             */
            std::shared_ptr<adapter::ProxyAdapter> proxy(const std::string& name);

            /**
             * @brief 设置规则列表
             * 
             * @param rules 规则列表
             */
            void setRules(std::vector<std::shared_ptr<rule::Rule>> rules);

            /**
             * @brief 设置隧道模式
             * 
             * @param mode 隧道模式
             */
            void setMode(Mode mode);

            /**
             * @brief 设置 DNS 解析器
             * 
             * @param resolver DNS 解析器指针
             */
            void setResolver(std::shared_ptr<dns::Resolver> resolver);

            /**
             * @brief 根据元数据匹配代理适配器
             * 
             * @param metadata 连接元数据
             * @return std::shared_ptr<adapter::ProxyAdapter> 匹配到的代理适配器
             */
            std::shared_ptr<adapter::ProxyAdapter> match(const constant::Metadata& metadata);

            /**
             * @brief 获取所有代理适配器
             * 
             * @return const std::map<std::string, std::shared_ptr<adapter::ProxyAdapter>>& 代理适配器映射
             */
            const std::map<std::string, std::shared_ptr<adapter::ProxyAdapter>>& proxies() const
            {
                return proxies_;
            }

        private:
            std::map<std::string, std::shared_ptr<adapter::ProxyAdapter>> proxies_;
            std::vector<std::shared_ptr<rule::Rule>> rules_;
            Mode mode_ = Mode::Rule;
            std::shared_ptr<dns::Resolver> resolver_;
        };

    } // namespace tunnel
} // namespace clash
