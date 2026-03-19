#include "runtime/log/log_formatter.h"
#include "runtime/log/logger.h"

int main() {
    auto& logger = runtime::log::Logger::instance();

    logger.setLogLevel(runtime::log::LogLevel::DEBUG);
    LOG_DEBUG("debug message, value=%d", 7);
    LOG_INFO("server started on port %d", 8080);
    LOG_ERROR("request failed: %s", "timeout");

    logger.setLogLevel(runtime::log::LogLevel::ERROR);
    LOG_INFO("this info message should be filtered");
    LOG_FATAL("fatal message for shutdown path");

    return 0;
}
