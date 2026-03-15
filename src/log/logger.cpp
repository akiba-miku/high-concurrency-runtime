#include "runtime/log/logger.h"

#include <iostream>
#include <ostream>

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
