#pragma once

#include "runtime/log/logger.h"
#include "runtime/time/timestamp.h"

#include <cstdio>
#include <string>

namespace runtime::log {

// 日志级别转字符串，供格式化函数和后续输出后端复用。
inline const char *ToString(LogLevel level) {
    switch(level) {
        case LogLevel::DEBUG:
            return "DEBUG";
        case LogLevel::INFO:
            return "INFO";
        case LogLevel::ERROR:
            return "ERROR";
        case LogLevel::FATAL:
            return "FATAL";
        default:
            return "UNKNOWN";
    }
}

inline runtime::base::String FormatLogMessage(
    LogLevel level,
    const char* file,
    int line,
    const char* func,
    const runtime::base::String& message) {
    // 时间戳逻辑统一下沉到 Timestamp，日志层只负责拼接展示格式。
    const auto now = runtime::time::Timestamp::now();
    return '[' + now.toFormattedString() + "] "
        + '[' + runtime::base::String(ToString(level)) + "] "
        + '[' + runtime::base::String(file) + ':' + std::to_string(line) + "] "
        + '[' + runtime::base::String(func) + "] "
        + message;
}

template <typename... Args>
inline runtime::base::String FormatPrintf(const char* format, Args... args) {
    // 先计算长度再分配缓冲区，避免固定数组截断日志内容。
    int size = std::snprintf(nullptr, 0, format, args...);
    if (size <= 0) {
        return {};
    }

    std::string buffer(static_cast<std::size_t>(size) + 1, '\0');
    std::snprintf(buffer.data(), buffer.size(), format, args...);
    buffer.resize(static_cast<std::size_t>(size));
    return buffer;
}

}  // namespace runtime::log

// 统一宏入口
#define RUNTIME_LOG(level, fmt, ...)                                                \
    do {                                                                            \
        ::runtime::log::Logger& logger = ::runtime::log::Logger::instance();        \
        logger.write(                                                               \
            level,                                                                  \
            ::runtime::log::FormatLogMessage(                                       \
                level,                                                              \
                __FILE__,                                                           \
                __LINE__,                                                           \
                __func__,                                                           \
                ::runtime::log::FormatPrintf(fmt, ##__VA_ARGS__)));                 \
    } while (0)

#define LOG_DEBUG(fmt, ...) RUNTIME_LOG(::runtime::log::LogLevel::DEBUG, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) RUNTIME_LOG(::runtime::log::LogLevel::INFO, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) RUNTIME_LOG(::runtime::log::LogLevel::ERROR, fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...) RUNTIME_LOG(::runtime::log::LogLevel::FATAL, fmt, ##__VA_ARGS__)
