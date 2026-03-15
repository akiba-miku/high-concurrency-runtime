#pragma once

#include "runtime/base/noncopyable.h"
#include "runtime/base/types.h"

#include <mutex>

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
