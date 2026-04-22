#include "runtime/config/config.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace runtime::config {

namespace {

std::string Trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return {};
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

const char* Env(const char* name) {
    const char* v = std::getenv(name);
    return v ? v : "";
}

void ApplyKV(Config& cfg, const std::string& key, const std::string& value) {
    if (key == "server.host")                 cfg.server.host = value;
    else if (key == "server.port")            cfg.server.port = static_cast<uint16_t>(std::stoi(value));
    else if (key == "server.io_threads")      cfg.server.io_threads = std::stoi(value);
    else if (key == "server.edge_triggered")  cfg.server.edge_triggered = (value == "true" || value == "1");
    else if (key == "server.request_timeout_ms") cfg.server.request_timeout_ms = std::stoi(value);
    else if (key == "log.level")              cfg.log.level = value;
    else if (key == "log.file")               cfg.log.file = value;
    else if (key == "log.rotate_mb")          cfg.log.rotate_mb = static_cast<std::size_t>(std::stoul(value));
    else if (key == "inference.model_path")   cfg.inference.model_path = value;
    else if (key == "inference.n_ctx")        cfg.inference.n_ctx = std::stoi(value);
    else if (key == "inference.n_gpu_layers") cfg.inference.n_gpu_layers = std::stoi(value);
    else if (key == "inference.n_batch")      cfg.inference.n_batch = std::stoi(value);
    else if (key == "inference.max_concurrency") cfg.inference.max_concurrency = std::stoi(value);
}

}  // namespace

Config Config::LoadFromFile(std::string_view path) {
    Config cfg;
    std::ifstream f{std::string(path)};
    if (!f) {
        throw std::runtime_error(
            std::string("Config: cannot open file: ") + std::string(path));
    }
    std::string line;
    while (std::getline(f, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = Trim(line.substr(0, eq));
        std::string val = Trim(line.substr(eq + 1));
        try {
            ApplyKV(cfg, key, val);
        } catch (...) {
            // skip malformed values silently
        }
    }
    return cfg;
}

Config Config::LoadFromEnv() {
    return ApplyEnv(Config{});
}

Config Config::ApplyEnv(Config cfg) {
    auto apply = [&](const char* env_key, const std::string& cfg_key) {
        const char* v = std::getenv(env_key);
        if (v && *v) {
            try {
                ApplyKV(cfg, cfg_key, std::string(v));
            } catch (...) {}
        }
    };

    apply("RUNTIME_SERVER_HOST",                "server.host");
    apply("RUNTIME_SERVER_PORT",                "server.port");
    apply("RUNTIME_SERVER_IO_THREADS",          "server.io_threads");
    apply("RUNTIME_SERVER_EDGE_TRIGGERED",      "server.edge_triggered");
    apply("RUNTIME_SERVER_REQUEST_TIMEOUT_MS",  "server.request_timeout_ms");
    apply("RUNTIME_LOG_LEVEL",                  "log.level");
    apply("RUNTIME_LOG_FILE",                   "log.file");
    apply("RUNTIME_INFERENCE_MODEL_PATH",       "inference.model_path");
    apply("RUNTIME_INFERENCE_N_CTX",            "inference.n_ctx");
    apply("RUNTIME_INFERENCE_N_GPU_LAYERS",     "inference.n_gpu_layers");
    apply("RUNTIME_INFERENCE_N_BATCH",          "inference.n_batch");
    apply("RUNTIME_INFERENCE_MAX_CONCURRENCY",  "inference.max_concurrency");

    return cfg;
}

}  // namespace runtime::config
