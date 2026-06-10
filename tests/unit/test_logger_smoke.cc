#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "runtime/log/async_logger.h"
#include "runtime/log/log_buffer.h"
#include "runtime/log/logger.h"
#include "runtime/trace/trace_context.h"

namespace {

bool Expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "[FAIL] " << message << '\n';
        return false;
    }
    return true;
}

std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream input(path);
    return std::string((std::istreambuf_iterator<char>(input)),
                       std::istreambuf_iterator<char>());
}

std::filesystem::path UniqueLogPath(const std::string& name) {
    const auto tick =
        std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() /
           ("runtime-" + name + "-" + std::to_string(tick) + ".log");
}

bool TestLogBuffer() {
    runtime::log::LogBuffer<8> buffer;
    if (!Expect(buffer.Empty(), "new log buffer should be empty")) return false;
    if (!Expect(buffer.Append("abc", 3), "append should succeed when space is available")) return false;
    if (!Expect(buffer.size() == 3U, "buffer size should reflect appended bytes")) return false;
    if (!Expect(!buffer.Append("012345", 6), "append should fail when space is insufficient")) return false;

    buffer.Reset();
    if (!Expect(buffer.Empty(), "reset should make the buffer empty again")) return false;
    return true;
}

bool TestFormatter() {
    const auto path = UniqueLogPath("formatter-smoke");
    auto& logger = runtime::log::Logger::Instance();
    logger.Init(path.string(), runtime::log::LogLevel::DEBUG, 10);
    logger.Log(runtime::log::LogLevel::WARN, "test_logger_smoke.cc", 88,
               "Formatter", "formatted message");
    logger.Shutdown();

    const std::string formatted = ReadFile(path);
    std::filesystem::remove(path);

    if (!Expect(formatted.find("[WARN]") != std::string::npos,
                "formatted log should contain the level")) return false;
    if (!Expect(formatted.find("[tid:") != std::string::npos,
                "formatted log should contain thread id")) return false;
    if (!Expect(formatted.find("[test_logger_smoke.cc:88]") != std::string::npos,
                "formatted log should contain file and line")) return false;
    if (!Expect(formatted.find("formatted message") != std::string::npos,
                "formatted log should contain the message body")) return false;
    if (!Expect(!formatted.empty() && formatted.back() == '\n',
                "formatted log should end with a newline")) return false;
    return true;
}

bool TestLoggerIntegration() {
    const auto path = UniqueLogPath("logger-smoke");
    auto& logger = runtime::log::Logger::Instance();

    logger.Init(path.string(), runtime::log::LogLevel::WARN, 10);
    logger.Log(runtime::log::LogLevel::INFO, "test_logger_smoke.cc", 120,
               "Integration", "filtered");
    LOG_ERROR() << "persisted error";
    const std::string huge(runtime::log::AsyncLogger::kBufferSize + 128, 'x');
    logger.Log(runtime::log::LogLevel::ERROR, "test_logger_smoke.cc", 123,
               "Integration", huge);
    logger.Shutdown();

    const std::string output = ReadFile(path);
    std::filesystem::remove(path);

    if (!Expect(output.find("filtered") == std::string::npos,
                "messages below threshold should be filtered")) return false;
    if (!Expect(output.find("persisted error") != std::string::npos,
                "error message should be persisted")) return false;
    if (!Expect(output.find("[ERROR]") != std::string::npos,
                "persisted log should include level metadata")) return false;
    if (!Expect(output.find("[truncated]") != std::string::npos,
                "oversized log should contain truncation marker")) return false;
    return true;
}

bool TestTraceCorrelation() {
    const auto path = UniqueLogPath("trace-corr-smoke");
    auto& logger = runtime::log::Logger::Instance();
    logger.Init(path.string(), runtime::log::LogLevel::DEBUG, 10);

    const auto ctx = runtime::trace::NewRootContext();
    {
        runtime::trace::ScopedTrace scoped{ctx};
        LOG_INFO() << "inside trace scope";
    }
    LOG_INFO() << "outside trace scope";
    logger.Shutdown();

    const std::string output = ReadFile(path);
    std::filesystem::remove(path);

    const std::string expected =
        "[trace:" + ctx.TraceIdHex() + "/" + ctx.SpanIdHex() + "]";
    if (!Expect(output.find(expected) != std::string::npos,
                "in-scope log should carry the trace correlation tag")) {
        return false;
    }
    const auto outside_pos = output.find("outside trace scope");
    if (!Expect(outside_pos != std::string::npos,
                "out-of-scope log should be persisted")) return false;
    // 作用域外的那一行不应再出现 [trace:
    const auto line_start = output.rfind('\n', outside_pos);
    const std::string outside_line = output.substr(
        line_start == std::string::npos ? 0 : line_start + 1,
        outside_pos - (line_start == std::string::npos ? 0 : line_start + 1));
    if (!Expect(outside_line.find("[trace:") == std::string::npos,
                "out-of-scope log should not carry a trace tag")) return false;
    return true;
}

}  // namespace

int main() {
    try {
        if (!TestLogBuffer()) return 1;
        if (!TestFormatter()) return 1;
        if (!TestLoggerIntegration()) return 1;
        if (!TestTraceCorrelation()) return 1;
    } catch (const std::exception& ex) {
        std::cerr << "[FAIL] unexpected exception: " << ex.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "[FAIL] unexpected unknown exception\n";
        return 1;
    }

    std::cout << "[PASS] logger_smoke_test\n";
    return 0;
}
