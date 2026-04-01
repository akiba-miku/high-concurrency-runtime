#pragma once

#include "runtime/base/noncopyable.h"

#include <atomic>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>

namespace runtime::log {

class AsyncLogger;

// 日志级别 按严重程度从低到高排列，做阈值过滤。
enum class LogLevel { 
  DEBUG = 0, 
  INFO, 
  WARN, 
  ERROR, 
  FATAL 
};

class Logger : public runtime::base::NonCopyable {
public:
  static Logger &Instance();

  void Init(const std::string &filename, 
            LogLevel level = LogLevel::INFO,
            int flush_interval_ms = 1000,
            std::size_t roll_size = 10 * 1024 * 1024);

  void Shutdown();

  void SetLogLevel(LogLevel level);
  LogLevel GetLogLevel() const;
  bool ShouldLog(LogLevel level) const;

  // 供外部调用的日志接口
  // 1. 日志级别的过滤
  // 2. 调用 formatter 格式化和生成完整日志行
  // 3. 交给 AsyncLogger 异步写入
  void Log(LogLevel level, 
           const char *file, 
           int line, 
           const char *func,
           std::string_view message);

private:
  Logger() = default;

private:
  std::atomic<LogLevel> level_{LogLevel::INFO};
  std::unique_ptr<AsyncLogger> async_logger_;
};

const char *ToString(LogLevel level);

class LogMessage : public runtime::base::NonCopyable {
public:
  LogMessage(LogLevel level, const char *file, int line, const char *func)
      : level_(level), file_(file), line_(line), func_(func) {}

  ~LogMessage() { Logger::Instance().Log(level_, file_, line_, func_, stream_.str()); }

  std::ostringstream &Stream() { return stream_; }

private:
  LogLevel level_;
  const char *file_;
  int line_;
  const char *func_;
  std::ostringstream stream_;
};

} // namespace runtime::log


#define LOG_DEBUG()                                                                   \
  if (!::runtime::log::Logger::Instance().ShouldLog(::runtime::log::LogLevel::DEBUG)) \
    ;                                                                                 \
  else                                                                                \
    ::runtime::log::LogMessage(::runtime::log::LogLevel::DEBUG, __FILE__, __LINE__,   \
                               __func__)                                               \
        .Stream()

#define LOG_INFO()                                                                   \
  if (!::runtime::log::Logger::Instance().ShouldLog(::runtime::log::LogLevel::INFO)) \
    ;                                                                                \
  else                                                                               \
    ::runtime::log::LogMessage(::runtime::log::LogLevel::INFO, __FILE__, __LINE__,   \
                               __func__)                                              \
        .Stream()

#define LOG_WARN()                                                                   \
  if (!::runtime::log::Logger::Instance().ShouldLog(::runtime::log::LogLevel::WARN)) \
    ;                                                                                \
  else                                                                               \
    ::runtime::log::LogMessage(::runtime::log::LogLevel::WARN, __FILE__, __LINE__,   \
                               __func__)                                              \
        .Stream()

#define LOG_ERROR()                                                                   \
  if (!::runtime::log::Logger::Instance().ShouldLog(::runtime::log::LogLevel::ERROR)) \
    ;                                                                                 \
  else                                                                                \
    ::runtime::log::LogMessage(::runtime::log::LogLevel::ERROR, __FILE__, __LINE__,   \
                               __func__)                                               \
        .Stream()

#define LOG_FATAL()                                                                   \
  if (!::runtime::log::Logger::Instance().ShouldLog(::runtime::log::LogLevel::FATAL)) \
    ;                                                                                 \
  else                                                                                \
    ::runtime::log::LogMessage(::runtime::log::LogLevel::FATAL, __FILE__, __LINE__,   \
                               __func__)                                               \
        .Stream()
