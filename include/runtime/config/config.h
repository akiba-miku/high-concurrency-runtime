#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace runtime::config {

struct ServerConfig {
    std::string host{"0.0.0.0"};
    uint16_t port{8080};
    int io_threads{0};           // 0 = hardware_concurrency
    bool edge_triggered{true};
    int request_timeout_ms{30000};
};

struct LogConfig {
    std::string level{"INFO"};
    std::string file{""};
    std::size_t rotate_mb{64};
};

struct InferenceConfig {
    std::string model_path{""};
    int n_ctx{4096};
    int n_gpu_layers{99};
    int n_batch{512};
    int max_concurrency{4};
};

struct Config {
    ServerConfig server;
    LogConfig log;
    InferenceConfig inference;

    // Load from KEY=VALUE file (# comments, unknown keys ignored).
    static Config LoadFromFile(std::string_view path);

    // Apply RUNTIME_* environment variable overrides.
    static Config LoadFromEnv();

    // LoadFromFile if path non-empty, then overlay env vars.
    static Config Load(std::string_view path = "") {
        Config cfg = path.empty() ? Config{} : LoadFromFile(path);
        return ApplyEnv(std::move(cfg));
    }

    void Validate() const {
        if (inference.model_path.empty())
            throw std::runtime_error("Config: inference.model_path is required");
    }

private:
    static Config ApplyEnv(Config cfg);
};

}  // namespace runtime::config
