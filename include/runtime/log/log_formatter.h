#pragma once

#include "runtime/base/current_thread.h"
#include "runtime/log/logger.h"
#include "runtime/time/timestamp.h"

#include <string>
#include <string_view>

namespace runtime::log {

inline std::string FormatLogMessage(
    LogLevel level,
    const char* file,
    int line,
    const char* func,
    std::string_view message) {
  const auto now = runtime::time::Timestamp::Now();

  return '[' + now.ToFormattedString() + "] "
      + '[' + std::string(ToString(level)) + "] "
      + "[tid:" + std::to_string(runtime::base::tid()) + "] "
      + '[' + std::string(file) + ':' + std::to_string(line) + "] "
      + '[' + std::string(func) + "] "
      + std::string(message) + '\n';
}

}  // namespace runtime::log
