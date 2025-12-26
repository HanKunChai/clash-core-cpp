#pragma once

#include <string>

namespace clash
{
    namespace constant
    {
        /**
         * @brief 路径管理类
         * 
         * 管理应用程序的主目录、配置文件路径和资源文件路径。
         */
        class Path
        {
        public:
            /**
             * @brief 获取单例实例
             * 
             * @return Path& 单例引用
             */
            static Path& instance();

            /**
             * @brief 设置主目录
             * 
             * @param root 主目录路径
             */
            void setHomeDir(const std::string& root);

            /**
             * @brief 设置配置文件名
             * 
             * @param file 配置文件名
             */
            void setConfig(const std::string& file);

            /**
             * @brief 获取主目录
             * 
             * @return std::string 主目录路径
             */
            std::string getHomeDir() const;

            /**
             * @brief 获取配置文件名
             * 
             * @return std::string 配置文件名
             */
            std::string getConfig() const;

            /**
             * @brief 解析相对路径
             * 
             * 将相对路径转换为基于主目录的绝对路径。
             * 
             * @param path 相对路径
             * @return std::string 绝对路径
             */
            std::string resolve(const std::string& path) const;

            /**
             * @brief 获取 MMDB 数据库路径
             * 
             * @return std::string MMDB 文件路径
             */
            std::string getMMDBPath() const;

        private:
            Path();
            std::string homeDir;
            std::string configFile;
        };

    } // namespace constant
} // namespace clash
