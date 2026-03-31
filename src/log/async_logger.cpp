#include "runtime/log/async_logger.h"

#include <chrono>
#include <cstring>
#include <string_view>
#include <utility>

namespace runtime::log {

namespace {

constexpr std::string_view kTruncatedSuffix = "... [truncated]\n";

}  // namespace

AsyncLogger::AsyncLogger(std::string filename, int flush_interval_ms,
                         std::size_t roll_size)
    : filename_(std::move(filename)), flush_interval_ms_(flush_interval_ms),
      roll_size_(roll_size), current_buffer_(std::make_unique<Buffer>()),
      next_buffer_(std::make_unique<Buffer>()) {}

AsyncLogger::~AsyncLogger() { Stop(); }

void AsyncLogger::Start() {
  if (started_) {
    return;
  }

  OpenFile();
  started_ = true;
  backend_thread_ = std::jthread([this](std::stop_token stop_token) {
    ThreadFunc(stop_token);
  });
}

void AsyncLogger::Stop() {
  if (!started_) {
    return;
  }

  backend_thread_.request_stop();
  cv_.notify_one();

  if (backend_thread_.joinable()) {
    backend_thread_.join();
  }

  CloseFile();
  started_ = false;
}

void AsyncLogger::Append(const char *data, std::size_t len) {
  if (data == nullptr || len == 0) {
    return;
  }

  const char* write_data = data;
  std::size_t write_len = len;
  std::string truncated_storage;

  if (len > kBufferSize) {
    const std::size_t suffix_len = kTruncatedSuffix.size();
    const std::size_t prefix_len = suffix_len >= kBufferSize ? 0 : (kBufferSize - suffix_len);

    truncated_storage.reserve(prefix_len + suffix_len);
    truncated_storage.append(data, prefix_len);
    truncated_storage.append(kTruncatedSuffix.data(), suffix_len);

    write_data = truncated_storage.data();
    write_len = truncated_storage.size();
  }

  std::lock_guard<std::mutex> lk{mutex_};

  // 尝试追加到当前缓冲区， 如果失败了说明当前缓冲区空间不足了。
  if (current_buffer_->Append(write_data, write_len)) {
    return;
  }

  // 将缓冲区加入待写入队列。
  buffers_.push_back(std::move(current_buffer_));

  if (next_buffer_) {
    current_buffer_ = std::move(next_buffer_);
  } else {
    current_buffer_ = std::make_unique<Buffer>();
  }

  if (!current_buffer_->Append(write_data, write_len)) {
    // 单条日志最多截断到一个 buffer 大小，理论上不应失败；失败时静默丢弃比写坏内存更安全。
    return;
  }
  cv_.notify_one();
}

void AsyncLogger::OpenFile() {
  // 如果没指定文件名 默认到标准输出
  if (filename_.empty()) {
    file_ = stdout;
    return;
  }

  file_ = std::fopen(filename_.c_str(), "a");
  // 文件打开失败则回退到标准输出
  if (file_ == nullptr) {
    file_ = stdout;
  }
}

void AsyncLogger::CloseFile() {
  if (file_ == nullptr) {
    return;
  }
  std::fflush(file_);
  
  if (file_ != stdout) {
    std::fclose(file_);
  }
  file_ = nullptr;
}

void AsyncLogger::FlushFile() {
  if (file_ != nullptr) {
    std::fflush(file_);
  }
}

void AsyncLogger::WriteBuffer(const Buffer &buffer) {
  if (file_ == nullptr || buffer.Empty()) {
    return;
  }
  std::size_t size = buffer.Size();
  std::size_t written = 0;
  while (written < size) {
    std::size_t n = std::fwrite(buffer.Data() + written, 1, size - written, file_);
    if (n > 0) {
      written += n;
      written_bytes_ += n;
      continue;
    }

    if (std::ferror(file_) != 0) {
      std::clearerr(file_);
      break;
    }

    break;
  }
}

void AsyncLogger::RotateIfNeeded() {
  // ... 省略日志文件滚动逻辑，实际实现中会根据文件大小和时间戳进行滚动。
}

void AsyncLogger::ThreadFunc(std::stop_token stop_token) {
  BufferQueue buffers_to_write;
  buffers_to_write.reserve(16);

  auto spare_buffer1 = std::make_unique<Buffer>();
  auto spare_buffer2 = std::make_unique<Buffer>();

  while (!stop_token.stop_requested()) {
    {
      std::unique_lock<std::mutex> lk{mutex_};

      if (buffers_.empty() &&
          (current_buffer_ == nullptr || current_buffer_->Empty())) {
        cv_.wait_for(lk, std::chrono::milliseconds(flush_interval_ms_));
      }

      if (current_buffer_ && !current_buffer_->Empty()) {
        buffers_.push_back(std::move(current_buffer_));
      }

      buffers_to_write.swap(buffers_);

      if (!current_buffer_) {
        if (spare_buffer1) {
          current_buffer_ = std::move(spare_buffer1);
        } else if (spare_buffer2) {
          current_buffer_ = std::move(spare_buffer2);
        } else {
          current_buffer_ = std::make_unique<Buffer>();
        }
      }

      if (!next_buffer_) {
        if (spare_buffer1) {
          next_buffer_ = std::move(spare_buffer1);
        } else if (spare_buffer2) {
          next_buffer_ = std::move(spare_buffer2);
        } else {
          next_buffer_ = std::make_unique<Buffer>();
        }
      }
    }

    if (buffers_to_write.empty()) {
      continue;
    }

    for (auto &buffer : buffers_to_write) {
      WriteBuffer(*buffer);
    }

    FlushFile();
    RotateIfNeeded();

    while (!spare_buffer1 && !buffers_to_write.empty()) {
      auto buffer = std::move(buffers_to_write.back());
      buffers_to_write.pop_back();
      if (buffer) {
        buffer->Reset();
        spare_buffer1 = std::move(buffer);
      }
    }

    while (!spare_buffer2 && !buffers_to_write.empty()) {
      auto buffer = std::move(buffers_to_write.back());
      buffers_to_write.pop_back();
      if (buffer) {
        buffer->Reset();
        spare_buffer2 = std::move(buffer);
      }
    }

    buffers_to_write.clear();
  }

  {
    std::lock_guard<std::mutex> lk{mutex_};
    if (current_buffer_ && !current_buffer_->Empty()) {
      buffers_.push_back(std::move(current_buffer_));
    }
    buffers_to_write.swap(buffers_);
  }

  for (auto &buffer : buffers_to_write) {
    WriteBuffer(*buffer);
  }

  FlushFile();
}
} // namespace runtime::log
