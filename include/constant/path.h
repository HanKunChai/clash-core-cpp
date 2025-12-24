#pragma once

#include <string>

namespace clash {
namespace constant {

class Path {
public:
    static Path& instance();

    void setHomeDir(const std::string& root);
    void setConfig(const std::string& file);

    std::string getHomeDir() const;
    std::string getConfig() const;
    std::string resolve(const std::string& path) const;
    std::string getMMDBPath() const;

private:
    Path();
    std::string homeDir;
    std::string configFile;
};

} // namespace constant
} // namespace clash
