#include "runtime/http/http_server.h"
#include "runtime/log/logger.h"
#include "runtime/net/event_loop.h"
#include "runtime/net/inet_address.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>

namespace {

std::string ReadEnvOrDefault(const char* key, const char* fallback) {
    const char* value = std::getenv(key);
    return value == nullptr ? std::string(fallback) : std::string(value);
}

bool ReadBoolEnvOrDefault(const char* key, bool fallback) {
    const char* value = std::getenv(key);
    if (value == nullptr) {
        return fallback;
    }

    std::string normalized(value);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return normalized == "1" || normalized == "true" || normalized == "on" ||
           normalized == "yes";
}

unsigned int ResolveThreadCount() {
    const char* value = std::getenv("IO_THREADS");
    if (value != nullptr) {
        return static_cast<unsigned int>(std::max(1, std::stoi(value)));
    }

    const unsigned int detected = std::thread::hardware_concurrency();
    if (detected == 0) {
        return 4;
    }
    return std::min(detected, 4u);
}

std::string JsonEscape(const std::string& input) {
    std::string out;
    out.reserve(input.size() + 8);
    for (char ch : input) {
        switch (ch) {
        case '\\':
            out += "\\\\";
            break;
        case '"':
            out += "\\\"";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            out.push_back(ch);
            break;
        }
    }
    return out;
}

}  // namespace

int main() {
    auto& logger = runtime::log::Logger::Instance();
    logger.Init("simple_http_server.log", runtime::log::LogLevel::INFO);

    const std::string host = ReadEnvOrDefault("HOST", "127.0.0.1");
    const std::uint16_t port = static_cast<std::uint16_t>(
        std::stoi(ReadEnvOrDefault("PORT", "18081")));
    const std::filesystem::path static_root = ReadEnvOrDefault("STATIC_ROOT", "examples/www");
    const bool keep_alive_enabled = ReadBoolEnvOrDefault("KEEP_ALIVE", true);
    const int idle_timeout_seconds = std::stoi(ReadEnvOrDefault("IDLE_TIMEOUT_SECONDS", "15"));

    runtime::net::EventLoop main_loop;
    runtime::http::HttpServer server(
        &main_loop,
        runtime::net::InetAddress(port, host),
        "SimpleHttpServer");

    const unsigned int thread_count = ResolveThreadCount();

    server.SetThreadNum(static_cast<int>(thread_count));
    server.SetIdleTimeout(std::chrono::seconds(idle_timeout_seconds));
    server.SetKeepAliveEnabled(keep_alive_enabled);
    server.SetStaticRoot(static_root);
    server.SetStaticUrlPrefix("/static/");

    server.Get("/", [](const runtime::http::HttpRequest&, runtime::http::HttpResponse* response) {
        response->SetContentType("text/plain; charset=utf-8");
        response->SetBody("runtime http server is up\n");
    });

    server.Get("/api/health",
               [](const runtime::http::HttpRequest& request,
                  runtime::http::HttpResponse* response) {
                   response->SetContentType("application/json; charset=utf-8");
                   response->SetBody(
                       "{\"ok\":true,\"path\":\"" + JsonEscape(request.Path()) + "\"}");
               });

    server.Post("/api/echo",
                [](const runtime::http::HttpRequest& request,
                   runtime::http::HttpResponse* response) {
                    response->SetContentType("application/json; charset=utf-8");
                    response->SetBody(
                        "{\"echo\":\"" + JsonEscape(request.Body()) + "\",\"query\":\"" +
                        JsonEscape(request.Query()) + "\"}");
                });

    server.Start();

    LOG_INFO() << "simple http server listening on " << host << ':' << port
               << " static_root=" << static_root.string()
               << " keep_alive=" << keep_alive_enabled
               << " idle_timeout_seconds=" << idle_timeout_seconds
               << " io_threads=" << thread_count;

    main_loop.Loop();
    return 0;
}
