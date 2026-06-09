// Copyright (c) 2026 Arsenova
// SPDX-License-Identifier: MIT
#include "runtime/trace/trace_recorder.h"

#include <algorithm>
#include <cstring>

namespace runtime::trace {

namespace {

// 截断拷贝进定长数组, 保证 NUL 结尾 (不用 snprintf, 无格式解析开销).
void CopyTruncated(char* dst, std::size_t dst_size, std::string_view src) {
  const std::size_t n = std::min(src.size(), dst_size - 1);
  std::memcpy(dst, src.data(), n);
  dst[n] = '\0';
}

// JSON 字符串字段的最小转义: 反斜杠、双引号与控制字符.
void AppendJsonEscaped(std::string& out, std::string_view s) {
  for (char c : s) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          static constexpr char kHex[] = "0123456789abcdef";
          out += "\\u00";
          out += kHex[(c >> 4) & 0xF];
          out += kHex[c & 0xF];
        } else {
          out += c;
        }
    }
  }
}

}  // namespace

const char* ToString(SpanKind kind) {
  switch (kind) {
    case SpanKind::kServer:
      return "server";
    case SpanKind::kClient:
      return "client";
    case SpanKind::kInternal:
      return "internal";
  }
  return "unknown";
}

void SpanRecord::set_name(std::string_view s) {
  CopyTruncated(name, sizeof(name), s);
}

void SpanRecord::set_attr(std::string_view s) {
  CopyTruncated(attr, sizeof(attr), s);
}

TraceRecorder::TraceRecorder() { ring_.resize(kDefaultCapacity); }

TraceRecorder& TraceRecorder::Instance() {
  static TraceRecorder instance;
  return instance;
}

void TraceRecorder::Record(const SpanRecord& span) {
  const double ratio = sample_ratio_.load(std::memory_order_relaxed);
  if (ratio < 1.0) {
    if (ratio <= 0.0) return;
    // 53 位均匀 double ∈ [0,1); 复用 trace 的线程局部 PRNG, 无锁.
    const double draw =
        static_cast<double>(RandomNonZeroU64() >> 11) * 0x1.0p-53;
    if (draw >= ratio) return;
  }
  recorded_total_.fetch_add(1, std::memory_order_relaxed);
  std::lock_guard lk{mu_};
  ring_[next_ % ring_.size()] = span;
  ++next_;
}

void TraceRecorder::set_sample_ratio(double ratio) {
  sample_ratio_.store(std::clamp(ratio, 0.0, 1.0),
                      std::memory_order_relaxed);
}

double TraceRecorder::sample_ratio() const {
  return sample_ratio_.load(std::memory_order_relaxed);
}

std::vector<SpanRecord> TraceRecorder::Snapshot() const {
  std::lock_guard lk{mu_};
  const std::size_t count = std::min(next_, ring_.size());
  std::vector<SpanRecord> out;
  out.reserve(count);
  // 从最新一条向旧回放 (next_ - 1, next_ - 2, ...).
  for (std::size_t i = 0; i < count; ++i) {
    out.push_back(ring_[(next_ - 1 - i) % ring_.size()]);
  }
  return out;
}

std::string TraceRecorder::RenderJson() const {
  const std::vector<SpanRecord> spans = Snapshot();
  std::string out;
  out.reserve(spans.size() * 256 + 64);
  out += "{\"recorded_total\":";
  out += std::to_string(recorded_total());
  out += ",\"spans\":[";
  bool first = true;
  for (const SpanRecord& s : spans) {
    if (!first) out += ',';
    first = false;
    out += "{\"trace_id\":\"";
    out += s.ctx.TraceIdHex();
    out += "\",\"span_id\":\"";
    out += s.ctx.SpanIdHex();
    out += "\",\"parent_span_id\":\"";
    if (s.ctx.parent_span_id != 0) {
      char buf[16];
      detail::WriteHexU64(s.ctx.parent_span_id, buf);
      out.append(buf, sizeof(buf));
    }
    out += "\",\"kind\":\"";
    out += ToString(s.kind);
    out += "\",\"start_us\":";
    out += std::to_string(s.start_us);
    out += ",\"duration_us\":";
    out += std::to_string(s.duration_us);
    out += ",\"status\":";
    out += std::to_string(s.status_code);
    out += ",\"sampled\":";
    out += s.ctx.Sampled() ? "true" : "false";
    out += ",\"name\":\"";
    AppendJsonEscaped(out, s.name);
    out += "\",\"attr\":\"";
    AppendJsonEscaped(out, s.attr);
    out += "\"}";
  }
  out += "]}";
  return out;
}

}  // namespace runtime::trace
