#pragma once

#include "adapter/proxy_adapter.h"
#include <vector>
#include <string>
#include <memory>
#include <algorithm>

namespace clash
{
    namespace adapter
    {
        /**
         * @brief 手动选择适配器
         * 
         * 允许用户通过 API 或 UI 手动选择使用的代理节点。
         */
        class SelectorAdapter : public ProxyAdapter
        {
        public:
            /**
             * @brief 构造函数
             * 
             * 初始化选择器，可选传入初始代理列表。
             * 
             * @param name 适配器名称
             * @param proxies 初始代理列表
             */
            SelectorAdapter(std::string name, std::vector<std::shared_ptr<ProxyAdapter>> proxies = {})
                : name_(std::move(name)), proxies_(std::move(proxies))
            {
                if (!proxies_.empty())
                {
                    selected_ = proxies_[0]; // 默认选择第一个
                }
            }

            /**
             * @brief 更新代理列表
             * 
             * 当配置重载或代理组更新时调用。
             * 
             * @param proxies 新的代理列表
             */
            void setProxies(std::vector<std::shared_ptr<ProxyAdapter>> proxies)
            {
                proxies_ = std::move(proxies);
                if (!proxies_.empty())
                {
                    // 尝试保持当前选择
                    if (selected_)
                    {
                        bool found = false;
                        for (const auto& p : proxies_)
                        {
                            if (p->name() == selected_->name())
                            {
                                found = true;
                                selected_ = p; // 更新引用，指向新列表中的对象
                                break;
                            }
                        }
                        // 如果当前选择的代理不在新列表中，重置为第一个
                        if (!found) selected_ = proxies_[0];
                    }
                    else
                    {
                        selected_ = proxies_[0];
                    }
                }
                else
                {
                    selected_ = nullptr;
                }
            }

            std::string name() const override
            {
                return name_;
            }

            constant::AdapterType type() const override
            {
                return constant::AdapterType::Selector;
            }

            /**
             * @brief 发起连接
             * 
             * 将流量转发给当前用户选中的代理节点。
             * 
             * @param metadata 连接元数据
             * @param io_context IO 上下文
             * @param handler 连接回调
             */
            void dial(const constant::Metadata& metadata, asio::io_context& io_context, ConnectHandler handler) override
            {
                if (selected_)
                {
                    selected_->dial(metadata, io_context, std::move(handler));
                }
                else
                {
                    // 如果没有选中任何代理（或列表为空），返回错误
                    handler(std::make_error_code(std::errc::destination_address_required), nullptr);
                }
            }

            /**
             * @brief 手动切换代理
             * 
             * 根据名称查找并设置为当前选中。
             * 
             * @param name 目标代理名称
             */
            void select(const std::string& name)
            {
                auto it = std::find_if(proxies_.begin(), proxies_.end(),
                    [&name](const std::shared_ptr<ProxyAdapter>& p)
                    {
                        return p->name() == name;
                    });
                
                if (it != proxies_.end())
                {
                    selected_ = *it;
                }
            }

            /**
             * @brief 获取当前选中的代理名称
             * 
             * @return std::string 当前选中的代理名称
             */
            std::string selected() const
            {
                return selected_ ? selected_->name() : "";
            }

            /**
             * @brief 获取所有可选代理的名称列表
             * 
             * @return std::vector<std::string> 代理名称列表
             */
            std::vector<std::string> all() const
            {
                std::vector<std::string> names;
                for (const auto& p : proxies_)
                {
                    names.push_back(p->name());
                }
                return names;
            }

        private:
            std::string name_;
            std::vector<std::shared_ptr<ProxyAdapter>> proxies_;
            std::shared_ptr<ProxyAdapter> selected_;
        };

    } // namespace adapter
} // namespace clash

