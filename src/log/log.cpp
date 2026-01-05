#include "log/log.h"
#include "constant/path.h"
#include <iostream>
#include <fstream>
#include <mutex>
#include <ctime>
#include <iomanip>
#include <map>
#include <atomic>
#include <filesystem>
#include <sstream>

namespace clash
{
    namespace log
    {

        static Level currentLevel = Level::Info;
        static std::mutex logMutex;
        static std::map<int, LogCallback> subscribers;
        static std::atomic<int> nextId{1};
        static std::ofstream logFile;

        void init()
        {
            std::lock_guard<std::mutex> lock(logMutex);
            try
            {
                std::string path = constant::Path::instance().resolve("clash.log");
                std::filesystem::path p(path);
                if (p.has_parent_path())
                {
                    std::filesystem::create_directories(p.parent_path());
                }
                // Open with trunc to clear file
                logFile.open(path, std::ios::trunc | std::ios::out);
                if (logFile.is_open())
                {
                    logFile.close();
                }
            }
            catch (...)
            {
                std::cerr << "Failed to init log file" << std::endl;
            }
        }

        void setLevel(Level level)
        {
            currentLevel = level;
        }

        Level getLevel()
        {
            return currentLevel;
        }

        int subscribe(LogCallback callback)
        {
            std::lock_guard<std::mutex> lock(logMutex);
            int id = nextId++;
            subscribers[id] = std::move(callback);
            return id;
        }

        void unsubscribe(int id)
        {
            std::lock_guard<std::mutex> lock(logMutex);
            subscribers.erase(id);
        }

        void print(Level level, const char* file, int line, const std::string& msg)
        {
            // 如果日志级别低于当前级别，则忽略
            if (level < currentLevel)
            {
                return;
            }

            std::lock_guard<std::mutex> lock(logMutex);

            // 如果日志文件未打开，尝试打开它
            if (!logFile.is_open())
            {
                try
                {
                    std::string path = constant::Path::instance().resolve("clash.log");
                    // 确保目录存在
                    std::filesystem::path p(path);
                    if (p.has_parent_path())
                    {
                        std::filesystem::create_directories(p.parent_path());
                    }
                    logFile.open(path, std::ios::app);
                }
                catch (...)
                {
                    // 如果文件打开失败，回退到标准错误输出
                    std::cerr << "Failed to open log file" << std::endl;
                }
            }

            std::time_t t = std::time(nullptr);
            std::tm now;
#ifdef _WIN32
            localtime_s(&now, &t);
#else
            localtime_r(&t, &now);
#endif

            std::string levelStr;
            switch (level)
            {
                case Level::Debug: levelStr = "DEBUG"; break;
                case Level::Info: levelStr = "INFO"; break;
                case Level::Warning: levelStr = "WARNING"; break;
                case Level::Error: levelStr = "ERROR"; break;
                case Level::Silent: return;
            }

            char timeBuf[32];
            std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &now);

            // 格式化日志消息：[时间] [级别] [函数名:行号] 消息
            std::stringstream ss;
            ss << "[" << timeBuf << "] "
               << "[" << levelStr << "] "
               << "[" << file << ":" << line << "] "
               << msg;

            std::string finalMsg = ss.str();

            // 写入文件
            if (logFile.is_open())
            {
                logFile << finalMsg << std::endl;
            }

            // 仅将 ERROR 级别输出到控制台
            if (level == Level::Error)
            {
                std::cerr << finalMsg << std::endl;
            }

            // 通知订阅者
            for (const auto& [id, callback] : subscribers)
            {
                callback(level, msg);
            }
        }

    } // namespace log
} // namespace clash
