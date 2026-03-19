#pragma once

#include "runtime/base/noncopyable.h"
#include "runtime/base/types.h"

#include <mutex>
#include <iostream>
#include <ostream>

namespace runtime::log {

// 日志级别 按严重程度从低到高排列，做阈值过滤。
enum class LogLevel {
    DEBUG,
    INFO,
    ERROR,
    FATAL
};

// 第一版先保留单例模式
class Logger : public runtime::base::NonCopyable {
public:
    static Logger& instance();

    void setLogLevel(LogLevel level);
    LogLevel logLevel() const;
    void write(LogLevel level, const runtime::base::String& msg);

private:
    Logger() = default;
    LogLevel logLevel_{LogLevel::INFO};
    mutable std::mutex mutex_;
};

}  // namespace runtime::log

namespace runtime::log {

namespace {

int LogLevelPriority(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:
            return 0;
        case LogLevel::INFO:
            return 1;
        case LogLevel::ERROR:
            return 2;
        case LogLevel::FATAL:
            return 3;
        default:
            return 0;
    }
}

}  // namespace

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::setLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    logLevel_ = level;
}

LogLevel Logger::logLevel() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return logLevel_;
}

void Logger::write(LogLevel level, const runtime::base::String& msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (LogLevelPriority(level) < LogLevelPriority(logLevel_)) {
        return;
    }

    // 选择输出位置
    std::ostream& out = (level == LogLevel::ERROR || level == LogLevel::FATAL)
        ? std::cerr
        : std::cout;
    out << msg << '\n';
}

}  // namespace runtime::log
