#include "log/log.h"
#include <iostream>
#include <mutex>
#include <ctime>
#include <iomanip>
#include <map>
#include <atomic>

namespace clash {
namespace log {

static Level currentLevel = Level::Info;
static std::mutex logMutex;
static std::map<int, LogCallback> subscribers;
static std::atomic<int> nextId{1};

void setLevel(Level level) {
    currentLevel = level;
}

Level getLevel() {
    return currentLevel;
}

int subscribe(LogCallback callback) {
    std::lock_guard<std::mutex> lock(logMutex);
    int id = nextId++;
    subscribers[id] = std::move(callback);
    return id;
}

void unsubscribe(int id) {
    std::lock_guard<std::mutex> lock(logMutex);
    subscribers.erase(id);
}

void print(Level level, const std::string& msg) {
    if (level < currentLevel) return;

    std::lock_guard<std::mutex> lock(logMutex);
    
    std::time_t t = std::time(nullptr);
    std::tm* now = std::localtime(&t);

    std::string levelStr;
    switch (level) {
        case Level::Debug: levelStr = "DEBUG"; break;
        case Level::Info: levelStr = "INFO"; break;
        case Level::Warning: levelStr = "WARNING"; break;
        case Level::Error: levelStr = "ERROR"; break;
        case Level::Silent: return;
    }

    std::cout << "[" << std::put_time(now, "%Y-%m-%d %H:%M:%S") << "] [" << levelStr << "] " << msg << std::endl;

    // Notify subscribers
    for (const auto& [id, callback] : subscribers) {
        callback(level, msg);
    }
}

void debugln(const std::string& msg) {
    print(Level::Debug, msg);
}

void infoln(const std::string& msg) {
    print(Level::Info, msg);
}

void warnln(const std::string& msg) {
    print(Level::Warning, msg);
}

void errorln(const std::string& msg) {
    print(Level::Error, msg);
}

} // namespace log
} // namespace clash
