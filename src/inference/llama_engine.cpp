#include "runtime/inference/llama_engine.h"

#include "runtime/log/logger.h"

#ifdef LLAMA_CPP_AVAILABLE
#  include "llama.h"
#endif

#include <stdexcept>
#include <string>
#include <vector>

namespace runtime::inference {

LlamaEngine::LlamaEngine(Config cfg)
    : config_(std::move(cfg)), queue_(/*capacity=*/256) {}

LlamaEngine::~LlamaEngine() {
    Stop();
}

void LlamaEngine::Start() {
    if (running_.exchange(true)) return;

#ifdef LLAMA_CPP_AVAILABLE
    llama_backend_init();

    llama_model_params mparams = llama_model_default_params();
    mparams.n_gpu_layers = config_.n_gpu_layers_;

    model = llama_load_model_from_file(config_.model_path.c_str(), mparams);
    if (!model) {
        throw std::runtime_error(
            "LlamaEngine: failed to load model: " + config_.model_path);
    }

    llama_context_params cparams = llama_context_default_params();
    cparams.n_ctx   = config_.n_ctx_;
    cparams.n_batch = config_.n_bacth_;

    ctx_ = llama_new_context_with_model(model, cparams);
    if (!ctx_) {
        throw std::runtime_error("LlamaEngine: failed to create context");
    }

    llama_sampler_chain_params sparams = llama_sampler_chain_default_params();
    sampler_ = llama_sampler_chain_init(sparams);
    llama_sampler_chain_add(sampler_, llama_sampler_init_top_p(0.95f, 1));
    llama_sampler_chain_add(sampler_, llama_sampler_init_temp(0.8f));
    llama_sampler_chain_add(sampler_,
                            llama_sampler_init_dist(LLAMA_DEFAULT_SEED));
#else
    LOG_WARN() << "LlamaEngine: built without LLAMA_CPP_AVAILABLE — stub mode";
#endif

    inference_thread_ = std::jthread([this](std::stop_token) {
        RunInferenceLoop();
    });

    LOG_INFO() << "LlamaEngine: started";
}

void LlamaEngine::Stop() {
    if (!running_.exchange(false)) return;
    queue_.Close();
    if (inference_thread_.joinable()) inference_thread_.join();

#ifdef LLAMA_CPP_AVAILABLE
    if (sampler_) { llama_sampler_free(sampler_); sampler_ = nullptr; }
    if (ctx_)     { llama_free(ctx_);             ctx_ = nullptr; }
    if (model)    { llama_free_model(model);       model = nullptr; }
    llama_backend_free();
#endif

    LOG_INFO() << "LlamaEngine: stopped";
}

void LlamaEngine::Submit(InferenceRequest req) {
    queue_.Push(std::move(req));
}

void LlamaEngine::RunInferenceLoop() {
    while (true) {
        auto maybe = queue_.Pop();
        if (!maybe) break;

        InferenceRequest req = std::move(*maybe);

        if (req.cancelled && req.cancelled->load()) {
            FireDone(req, "cancelled");
            continue;
        }

        try {
            ProcessOne(req);
        } catch (const std::exception& e) {
            LOG_ERROR() << "LlamaEngine: inference error: " << e.what();
            FireDone(req, "error");
        }
    }
}

static void FireCallback(InferenceRequest& req, std::function<void()> fn) {
    if (req.io_loop_) {
        req.io_loop_->RunInLoop(std::move(fn));
    } else {
        fn();
    }
}

void LlamaEngine::ProcessOne(InferenceRequest& req) {
#ifdef LLAMA_CPP_AVAILABLE
    llama_kv_cache_clear(ctx_);

    std::vector<llama_token> tokens(req.prompt_.size() + 4);
    int n = llama_tokenize(model,
                           req.prompt_.c_str(),
                           static_cast<int>(req.prompt_.size()),
                           tokens.data(),
                           static_cast<int>(tokens.size()),
                           /*add_special=*/true,
                           /*parse_special=*/false);
    if (n < 0) throw std::runtime_error("tokenize failed");
    tokens.resize(static_cast<std::size_t>(n));

    llama_batch batch = llama_batch_get_one(tokens.data(), n);
    if (llama_decode(ctx_, batch) != 0)
        throw std::runtime_error("llama_decode (prompt) failed");

    int generated = 0;
    while (generated < req.max_new_tokens_) {
        if (req.cancelled && req.cancelled->load()) break;

        llama_token id = llama_sampler_sample(sampler_, ctx_, -1);
        llama_sampler_accept(sampler_, id);
        if (llama_token_is_eog(model, id)) break;

        char buf[256];
        int piece_len = llama_token_to_piece(model, id, buf, sizeof(buf), false);
        if (piece_len > 0 && req.token_cb_) {
            std::string piece(buf, static_cast<std::size_t>(piece_len));
            auto cb = req.token_cb_;
            FireCallback(req, [cb, piece] { cb(piece); });
        }

        llama_batch next = llama_batch_get_one(&id, 1);
        if (llama_decode(ctx_, next) != 0) break;
        ++generated;
    }

    FireDone(req, generated >= req.max_new_tokens_ ? "length" : "stop");
#else
    if (req.token_cb_) {
        auto cb = req.token_cb_;
        std::string tok = "[llama.cpp not linked — stub response]";
        FireCallback(req, [cb, tok] { cb(tok); });
    }
    FireDone(req, "stop");
#endif
}

void LlamaEngine::FireDone(InferenceRequest& req, const std::string& reason) {
    if (!req.done_cb_) return;
    auto cb = req.done_cb_;
    FireCallback(req, [cb, reason] { cb(reason); });
}

}  // namespace runtime::inference
