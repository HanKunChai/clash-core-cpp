#include "rule/process.h"
#include "common/platform.h"
#include <filesystem>

namespace clash
{
    namespace rule
    {

        // 构造函数：初始化 Process 规则
        // processName: 目标进程名称 (例如 "curl", "chrome")
        // adapter: 匹配后使用的代理策略
        Process::Process(std::string processName, std::string adapter)
            : processName_(std::move(processName)), adapter_(std::move(adapter))
        {
        }

        constant::RuleType Process::type() const
        {
            return constant::RuleType::Process;
        }

        // 匹配逻辑：根据源端口查找进程，并比对进程名称
        bool Process::match(const constant::Metadata& metadata) const
        {
            // 如果 Metadata 中没有源端口，无法匹配
            if (metadata.srcPort == 0)
            {
                return false;
            }

            // 获取进程完整路径
            std::string path = common::Platform::getProcessPath(metadata.srcPort);
            if (path.empty())
            {
                return false;
            }

            // 提取文件名
            std::string name = std::filesystem::path(path).filename().string();

            // 简单的名称匹配 (Linux 下通常区分大小写)
            return name == processName_;
        }

        std::string Process::adapter() const
        {
            return adapter_;
        }

        std::string Process::payload() const
        {
            return processName_;
        }

        // 构造函数：初始化 ProcessPath 规则
        // processPath: 目标进程完整路径 (例如 "/usr/bin/curl")
        // adapter: 匹配后使用的代理策略
        ProcessPath::ProcessPath(std::string processPath, std::string adapter)
            : processPath_(std::move(processPath)), adapter_(std::move(adapter))
        {
        }

        constant::RuleType ProcessPath::type() const
        {
            return constant::RuleType::ProcessPath;
        }

        // 匹配逻辑：根据源端口查找进程，并比对进程完整路径
        bool ProcessPath::match(const constant::Metadata& metadata) const
        {
            if (metadata.srcPort == 0)
            {
                return false;
            }

            std::string path = common::Platform::getProcessPath(metadata.srcPort);
            if (path.empty())
            {
                return false;
            }

            // 路径匹配 (Linux 下区分大小写)
            return path == processPath_;
        }

        std::string ProcessPath::adapter() const
        {
            return adapter_;
        }

        std::string ProcessPath::payload() const
        {
            return processPath_;
        }

    } // namespace rule
} // namespace clash
