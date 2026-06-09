// Copyright (c) 2026 Arsenova
// SPDX-License-Identifier: MIT
#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "runtime/base/noncopyable.h"
#include "runtime/trace/trace_context.h"

// 进程内"最近 span"环形缓冲. 不做外部导出 (无 OTLP/第三方依赖),
// 定位是排障自省: 上层把它渲染到 /debug/traces 之类的端点.
namespace runtime::trace {

enum class SpanKind : std::uint8_t {
  kServer,    // 本进程作为服务端处理一次请求
  kClient,    // 本进程作为客户端调用上游
  kInternal,  // 进程内部阶段
};

[[nodiscard]] const char* ToString(SpanKind kind);

// 一条已完成 span 的记录. 定长 char 数组保证平凡可拷贝、热路径零分配;
// 超长内容截断 (NUL 结尾).
struct SpanRecord {
  TraceContext ctx;
  SpanKind kind{SpanKind::kInternal};
  std::uint64_t start_us{0};     // wall-clock epoch 微秒
  std::uint64_t duration_us{0};
  int status_code{0};            // HTTP 状态码, 0 = 不适用
  char name[48]{};               // 如 "GET /users/:id"
  char attr[48]{};               // 如 peer 名 / client ip

  void set_name(std::string_view s);
  void set_attr(std::string_view s);
};

// 互斥锁保护的固定容量环. 写者是每个完成的请求 (与 metrics::Histogram
// 的每请求一次加锁同量级); 读者只有偶发的 /debug 端点, 不值得分片.
class TraceRecorder : public runtime::base::NonCopyable {
public:
  static constexpr std::size_t kDefaultCapacity = 256;

  static TraceRecorder& Instance();

  // 按 sample_ratio 采样后追加到环 (环满覆盖最旧).
  void Record(const SpanRecord& span);

  // 写入环的采样比, [0.0, 1.0], 默认 1.0 (环有界, 全记录安全).
  // 只影响环内容; trace 的生成与传播不受采样控制.
  void set_sample_ratio(double ratio);
  [[nodiscard]] double sample_ratio() const;

  // 时间倒序 (最新在前) 拷出当前环内容.
  [[nodiscard]] std::vector<SpanRecord> Snapshot() const;

  // 渲染为 JSON 数组文本 (纯字符串拼接, foundation 不感知 HTTP 类型).
  [[nodiscard]] std::string RenderJson() const;

  // 历史累计记录条数 (含已被覆盖的).
  [[nodiscard]] std::uint64_t recorded_total() const {
    return recorded_total_.load(std::memory_order_relaxed);
  }

private:
  TraceRecorder();

  mutable std::mutex mu_;
  std::vector<SpanRecord> ring_;  // 预分配 kDefaultCapacity
  std::uint64_t next_{0};         // 单调写索引; 实际槽位 = next_ % capacity
  std::atomic<std::uint64_t> recorded_total_{0};
  std::atomic<double> sample_ratio_{1.0};
};

}  // namespace runtime::trace
