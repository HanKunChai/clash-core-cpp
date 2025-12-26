#include "constant/path.h"
#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace clash
{
    namespace constant
    {

        namespace fs = std::filesystem;

        Path& Path::instance()
        {
            static Path instance;
            return instance;
        }

        Path::Path()
        {
            const char* home = std::getenv("HOME");
            if (!home)
            {
                home = "."; // 回退到当前目录
            }
            // 强制转换为绝对路径，确保路径解析正确
            homeDir = fs::absolute(fs::path(home) / ".config" / "clash").string();
            configFile = "config.yaml";
        }

        void Path::setHomeDir(const std::string& root)
        {
            homeDir = root;
        }

        void Path::setConfig(const std::string& file)
        {
            configFile = file;
        }

        std::string Path::getHomeDir() const
        {
            return homeDir;
        }

        std::string Path::getConfig() const
        {
            return configFile;
        }

        std::string Path::resolve(const std::string& path) const
        {
            fs::path p(path);
            if (p.is_absolute())
            {
                return p.string();
            }
            return (fs::path(homeDir) / p).string();
        }

        std::string Path::getMMDBPath() const
        {
            return resolve("Country.mmdb");
        }

    } // namespace constant
} // namespace clash
