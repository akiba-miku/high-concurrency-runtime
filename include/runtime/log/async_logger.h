#pragma once

#include "runtime/base/noncopyable.h"
#include "runtime/log/log_buffer.h"

#include <condition_variable>
#include <cstdio>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>


namespace runtime::log {

class AsyncLogger : public runtime::base::NonCopyable {
public:
  static constexpr std::size_t kBufferSize = 64 * 1024; // 64KB
  using Buffer = LogBuffer<kBufferSize>;
  using BufferPtr = std::unique_ptr<Buffer>; // 通过 unique_ptr 避免拷贝
  using BufferQueue = std::vector<BufferPtr>;

  explicit AsyncLogger(std::string filename, 
                       int flush_interval_ms = 1000,
                       std::size_t roll_size = 10 * 1024 * 1024);
  ~AsyncLogger();

  void Start();
  void Stop();
  void Append(const char *data, std::size_t len);

private:
  void ThreadFunc(std::stop_token stop_token);

  void OpenFile();
  void CloseFile();
  void FlushFile();
  void WriteBuffer(const Buffer &buffer);
  void RotateIfNeeded();

private:
  std::string filename_;
  int flush_interval_ms_;
  std::size_t roll_size_;

  std::mutex mutex_;
  std::condition_variable_any cv_;

  // 双缓冲设计，current_buffer_ 前端写日志，next_buffer_ 作为备用缓冲区
  //buffers_ 存储待写入后端的日志数据。 
  BufferPtr current_buffer_;
  BufferPtr next_buffer_;
  BufferQueue buffers_;

  std::jthread backend_thread_;
  bool started_ {false};

  FILE *file_{nullptr};
  std::size_t written_bytes_{0};
};
} // namespace runtime::log