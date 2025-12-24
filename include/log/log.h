#pragma once

#include <string>
#include <iostream>
#include <memory>
#include <functional>

namespace clash {
namespace log {

enum class Level {
    Debug,
    Info,
    Warning,
    Error,
    Silent
};

using LogCallback = std::function<void(Level, const std::string&)>;

void setLevel(Level level);
Level getLevel();

// Subscribe to log events. Returns a subscription ID.
int subscribe(LogCallback callback);
// Unsubscribe using the ID.
void unsubscribe(int id);

void debugln(const std::string& msg);
void infoln(const std::string& msg);
void warnln(const std::string& msg);
void errorln(const std::string& msg);

// Helper to format strings (simplified version of fmt::format or sprintf)
template<typename ... Args>
std::string format(const std::string& format, Args ... args) {
    int size_s = std::snprintf(nullptr, 0, format.c_str(), args ...) + 1; // Extra space for '\0'
    if (size_s <= 0) { throw std::runtime_error("Error during formatting."); }
    auto size = static_cast<size_t>(size_s);
    std::unique_ptr<char[]> buf(new char[size]);
    std::snprintf(buf.get(), size, format.c_str(), args ...);
    return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
}

template<typename ... Args>
void debug(const std::string& fmt, Args ... args) {
    debugln(format(fmt, args...));
}

template<typename ... Args>
void info(const std::string& fmt, Args ... args) {
    infoln(format(fmt, args...));
}

template<typename ... Args>
void warn(const std::string& fmt, Args ... args) {
    warnln(format(fmt, args...));
}

template<typename ... Args>
void error(const std::string& fmt, Args ... args) {
    errorln(format(fmt, args...));
}

} // namespace log
} // namespace clash
