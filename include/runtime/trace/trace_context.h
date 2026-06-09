// Copyright (c) 2026 Arsenova
// SPDX-License-Identifier: MIT
#pragma once

#include <chrono>
#include <cstdint>
#include <cstring>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <string_view>

// W3C Trace Context (https://www.w3.org/TR/trace-context/) 的最小实现:
// 解析/生成 traceparent 头, 生成 trace_id/span_id, 以及一个 thread_local
// "当前请求 trace" 供日志关联使用. 全部 header-only, 仅依赖 stdlib,
// 位于 foundation 层 (任何上层均可 include, 反向不可).
namespace runtime::trace {

// "00-" + 32 hex + "-" + 16 hex + "-" + 2 hex
inline constexpr std::size_t kTraceparentLen = 55;

// 一次请求的追踪上下文. 平凡可拷贝的 POD 值 (无指针, 不进 arena),
// 全零 = invalid, 表示"没有 trace".
struct TraceContext {
  std::uint64_t trace_id_hi{0};
  std::uint64_t trace_id_lo{0};
  std::uint64_t span_id{0};
  std::uint64_t parent_span_id{0};
  std::uint8_t flags{0};  // bit0 = sampled (W3C trace-flags)

  [[nodiscard]] bool Valid() const {
    return (trace_id_hi | trace_id_lo) != 0 && span_id != 0;
  }
  [[nodiscard]] bool Sampled() const { return (flags & 0x01) != 0; }

  void WriteTraceparent(std::span<char, kTraceparentLen> out) const;
  [[nodiscard]] std::string ToTraceparent() const;
  [[nodiscard]] std::string TraceIdHex() const;  // 32 chars
  [[nodiscard]] std::string SpanIdHex() const;   // 16 chars
};

namespace detail {

inline constexpr char kHexDigits[] = "0123456789abcdef";

inline void WriteHexU64(std::uint64_t v, char* out) {
  for (int i = 15; i >= 0; --i) {
    out[i] = kHexDigits[v & 0xF];
    v >>= 4;
  }
}

// 单个小写/大写 hex 字符 -> 值; 非 hex 返回 -1.
inline int HexValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

// 解析定长 hex 段; 任一非 hex 字符返回 nullopt.
inline std::optional<std::uint64_t> ParseHexU64(std::string_view s) {
  std::uint64_t v = 0;
  for (char c : s) {
    int d = HexValue(c);
    if (d < 0) return std::nullopt;
    v = (v << 4) | static_cast<std::uint64_t>(d);
  }
  return v;
}

// SplitMix64: 种子扩展用 (xoshiro 作者推荐的 seeding 方式).
inline std::uint64_t SplitMix64(std::uint64_t& state) {
  std::uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31);
}

// xoshiro256++: 每线程一个实例, 无锁, 种子来自 random_device ⊕ 线程地址
// ⊕ steady_clock, 避免多线程/多进程撞 ID.
class Xoshiro256pp {
public:
  Xoshiro256pp() {
    std::random_device rd;
    std::uint64_t seed =
        (static_cast<std::uint64_t>(rd()) << 32) ^ rd() ^
        reinterpret_cast<std::uintptr_t>(this) ^
        static_cast<std::uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count());
    for (auto& s : s_) s = SplitMix64(seed);
  }

  std::uint64_t Next() {
    auto rotl = [](std::uint64_t x, int k) {
      return (x << k) | (x >> (64 - k));
    };
    const std::uint64_t result = rotl(s_[0] + s_[3], 23) + s_[0];
    const std::uint64_t t = s_[1] << 17;
    s_[2] ^= s_[0];
    s_[3] ^= s_[1];
    s_[1] ^= s_[2];
    s_[0] ^= s_[3];
    s_[2] ^= t;
    s_[3] = rotl(s_[3], 45);
    return result;
  }

private:
  std::uint64_t s_[4]{};
};

}  // namespace detail

// 线程局部 PRNG 取一个非零 u64 (trace_id/span_id 不允许全零).
[[nodiscard]] inline std::uint64_t RandomNonZeroU64() {
  thread_local detail::Xoshiro256pp rng;
  std::uint64_t v;
  do {
    v = rng.Next();
  } while (v == 0);
  return v;
}

inline void TraceContext::WriteTraceparent(
    std::span<char, kTraceparentLen> out) const {
  out[0] = '0';
  out[1] = '0';
  out[2] = '-';
  detail::WriteHexU64(trace_id_hi, out.data() + 3);
  detail::WriteHexU64(trace_id_lo, out.data() + 19);
  out[35] = '-';
  detail::WriteHexU64(span_id, out.data() + 36);
  out[52] = '-';
  out[53] = detail::kHexDigits[(flags >> 4) & 0xF];
  out[54] = detail::kHexDigits[flags & 0xF];
}

inline std::string TraceContext::ToTraceparent() const {
  char buf[kTraceparentLen];
  WriteTraceparent(std::span<char, kTraceparentLen>{buf});
  return std::string(buf, kTraceparentLen);
}

inline std::string TraceContext::TraceIdHex() const {
  char buf[32];
  detail::WriteHexU64(trace_id_hi, buf);
  detail::WriteHexU64(trace_id_lo, buf + 16);
  return std::string(buf, sizeof(buf));
}

inline std::string TraceContext::SpanIdHex() const {
  char buf[16];
  detail::WriteHexU64(span_id, buf);
  return std::string(buf, sizeof(buf));
}

// 解析 traceparent 头. 失败 (长度/分隔符/版本/非 hex/全零 id) 返回 nullopt.
// 只接受 version "00"; "ff" 是 W3C 保留的非法版本, 其余未知版本按保守策略
// 一并拒绝 (调用方会退化为新建 root trace, 行为安全).
[[nodiscard]] inline std::optional<TraceContext> ParseTraceparent(
    std::string_view header) {
  if (header.size() != kTraceparentLen) return std::nullopt;
  if (header[0] != '0' || header[1] != '0') return std::nullopt;
  if (header[2] != '-' || header[35] != '-' || header[52] != '-') {
    return std::nullopt;
  }
  auto hi = detail::ParseHexU64(header.substr(3, 16));
  auto lo = detail::ParseHexU64(header.substr(19, 16));
  auto span = detail::ParseHexU64(header.substr(36, 16));
  auto flags = detail::ParseHexU64(header.substr(53, 2));
  if (!hi || !lo || !span || !flags) return std::nullopt;
  if ((*hi | *lo) == 0 || *span == 0) return std::nullopt;
  TraceContext ctx;
  ctx.trace_id_hi = *hi;
  ctx.trace_id_lo = *lo;
  ctx.span_id = *span;
  ctx.parent_span_id = 0;
  ctx.flags = static_cast<std::uint8_t>(*flags);
  return ctx;
}

// 新建根 trace: 全新 trace_id + span_id, 默认 sampled.
[[nodiscard]] inline TraceContext NewRootContext() {
  TraceContext ctx;
  ctx.trace_id_hi = RandomNonZeroU64();
  ctx.trace_id_lo = RandomNonZeroU64();
  ctx.span_id = RandomNonZeroU64();
  ctx.parent_span_id = 0;
  ctx.flags = 0x01;
  return ctx;
}

// 派生子 span: 保留 trace_id 与 flags, 父 span 变 parent, 换新 span_id.
[[nodiscard]] inline TraceContext NewChildContext(const TraceContext& parent) {
  TraceContext ctx = parent;
  ctx.parent_span_id = parent.span_id;
  ctx.span_id = RandomNonZeroU64();
  return ctx;
}

namespace detail {
inline thread_local TraceContext g_current_trace{};
}  // namespace detail

// 当前线程正在处理的请求 trace; 无请求作用域时返回 invalid (全零).
// 日志格式化可读取它把 trace_id 写进日志行 (foundation 内闭环).
[[nodiscard]] inline const TraceContext& CurrentTrace() {
  return detail::g_current_trace;
}

// RAII 设置/恢复 thread_local 当前 trace. 可嵌套; 必须在执行 handler 的那个
// 线程上构造 (worker 线程派发场景下要放在投递的 lambda 体内).
class ScopedTrace {
public:
  explicit ScopedTrace(const TraceContext& ctx)
      : saved_(detail::g_current_trace) {
    detail::g_current_trace = ctx;
  }
  ~ScopedTrace() { detail::g_current_trace = saved_; }

  ScopedTrace(const ScopedTrace&) = delete;
  ScopedTrace& operator=(const ScopedTrace&) = delete;

private:
  TraceContext saved_;
};

}  // namespace runtime::trace
