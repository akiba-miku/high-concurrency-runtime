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

// 进程内"应用事件"环形日志 —— 可观测性的 Events 支柱.
// 记录的是离散的状态/变更事件 (服务启停、熔断跳变、节点上下线、限流触发),
// 不是请求级日志; 量极小但归因价值高 ("出问题前发生了什么变更?").
// 上层把它渲染到 /debug/events 端点; foundation 不感知 HTTP 类型.
//
// 本头文件被上层 (如 gateway 的 header-only 组件) 广泛包含, 保持轻量:
// 不引入 <sstream> / <chrono> 等重头文件.
namespace runtime::events {

enum class EventType : std::uint8_t {
  kProcessStart,
  kServerStart,
  kServerStop,
  kCircuitBreakerOpen,
  kCircuitBreakerHalfOpen,
  kCircuitBreakerClosed,
  kPeerUp,
  kPeerDown,
  kRateLimitTriggered,
  kCustom,
};

enum class EventSeverity : std::uint8_t {
  kInfo,
  kWarn,
  kError,
};

[[nodiscard]] const char* ToString(EventType type);
[[nodiscard]] const char* ToString(EventSeverity severity);

// 一条事件. 定长 char 数组保证平凡可拷贝、Emit 路径零分配; 超长截断.
struct AppEvent {
  std::uint64_t timestamp_us{0};  // wall-clock epoch 微秒
  EventType type{EventType::kCustom};
  EventSeverity severity{EventSeverity::kInfo};
  char subject[48]{};   // 事件主体, 如 "upstream_a/peer1"、"global"
  char message[96]{};   // 自由文本
};

// 互斥锁保护的固定容量环. 写者只在状态跳变时出现 (Hz 量级),
// 读者只有偶发的 /debug 端点 —— 锁开销不可测量.
class EventJournal : public runtime::base::NonCopyable {
public:
  static constexpr std::size_t kDefaultCapacity = 512;

  static EventJournal& Instance();

  // 追加一条事件 (环满覆盖最旧); timestamp 自动取当前时间.
  // subject/message 超长截断, 不分配, 不抛异常.
  void Emit(EventType type, EventSeverity severity, std::string_view subject,
            std::string_view message);

  // 时间倒序 (最新在前) 拷出当前环内容.
  [[nodiscard]] std::vector<AppEvent> Snapshot() const;

  // 渲染为 JSON 文本 (纯字符串拼接).
  [[nodiscard]] std::string RenderJson() const;

  // 历史累计事件数 (含已被覆盖的).
  [[nodiscard]] std::uint64_t emitted_total() const {
    return emitted_total_.load(std::memory_order_relaxed);
  }

private:
  EventJournal();

  mutable std::mutex mu_;
  std::vector<AppEvent> ring_;  // 预分配 kDefaultCapacity
  std::uint64_t next_{0};       // 单调写索引; 槽位 = next_ % capacity
  std::atomic<std::uint64_t> emitted_total_{0};
};

}  // namespace runtime::events
