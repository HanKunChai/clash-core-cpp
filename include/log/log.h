#pragma once

#include <string>
#include <iostream>
#include <memory>
#include <functional>

namespace clash
{
    namespace log
    {
        /**
         * @brief 日志级别枚举
         */
        enum class Level
        {
            Debug,   // 调试信息
            Info,    // 一般信息
            Warning, // 警告信息
            Error,   // 错误信息
            Silent   // 静默 (不输出)
        };

        /**
         * @brief 日志回调函数类型
         */
        using LogCallback = std::function<void(Level, const std::string&)>;

        /**
         * @brief 设置全局日志级别
         * 
         * @param level 日志级别
         */
        void setLevel(Level level);

        /**
         * @brief 获取全局日志级别
         * 
         * @return Level 日志级别
         */
        Level getLevel();

        /**
         * @brief 订阅日志事件
         * 
         * @param callback 回调函数
         * @return int 订阅 ID
         */
        int subscribe(LogCallback callback);

        /**
         * @brief 取消订阅日志事件
         * 
         * @param id 订阅 ID
         */
        void unsubscribe(int id);

        /**
         * @brief 内部打印函数
         * 
         * @param level 日志级别
         * @param func 函数名
         * @param line 行号
         * @param msg 消息内容
         */
        void print(Level level, const char* func, int line, const std::string& msg);

        /**
         * @brief 格式化字符串辅助函数
         * 
         * 类似于 sprintf，但返回 std::string。
         * 
         * @tparam Args 参数类型
         * @param format 格式化字符串
         * @param args 参数列表
         * @return std::string 格式化后的字符串
         */
        template<typename ... Args>
        std::string format(const std::string& format, Args ... args)
        {
            int size_s = std::snprintf(nullptr, 0, format.c_str(), args ...) + 1; // 为 '\0' 预留空间
            if (size_s <= 0)
            {
                throw std::runtime_error("Error during formatting.");
            }
            auto size = static_cast<size_t>(size_s);
            std::unique_ptr<char[]> buf(new char[size]);
            std::snprintf(buf.get(), size, format.c_str(), args ...);
            return std::string(buf.get(), buf.get() + size - 1); // 我们不想要内部的 '\0'
        }

    } // namespace log
} // namespace clash

// 用于捕获函数名和行号的宏
#define LOG_DEBUGLN(msg) clash::log::print(clash::log::Level::Debug, __FUNCTION__, __LINE__, msg)
#define LOG_INFOLN(msg)  clash::log::print(clash::log::Level::Info,  __FUNCTION__, __LINE__, msg)
#define LOG_WARNLN(msg)  clash::log::print(clash::log::Level::Warning, __FUNCTION__, __LINE__, msg)
#define LOG_ERRORLN(msg) clash::log::print(clash::log::Level::Error, __FUNCTION__, __LINE__, msg)

#define LOG_DEBUG(fmt, ...) clash::log::print(clash::log::Level::Debug, __FUNCTION__, __LINE__, clash::log::format(fmt, ##__VA_ARGS__))
#define LOG_INFO(fmt, ...)  clash::log::print(clash::log::Level::Info,  __FUNCTION__, __LINE__, clash::log::format(fmt, ##__VA_ARGS__))
#define LOG_WARN(fmt, ...)  clash::log::print(clash::log::Level::Warning, __FUNCTION__, __LINE__, clash::log::format(fmt, ##__VA_ARGS__))
#define LOG_ERROR(fmt, ...) clash::log::print(clash::log::Level::Error, __FUNCTION__, __LINE__, clash::log::format(fmt, ##__VA_ARGS__))
