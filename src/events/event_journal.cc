// Copyright (c) 2026 Arsenova
// SPDX-License-Identifier: MIT
#include "runtime/events/event_journal.h"

#include <algorithm>
#include <chrono>
#include <cstring>

namespace runtime::events {

namespace {

void CopyTruncated(char* dst, std::size_t dst_size, std::string_view src) {
  const std::size_t n = std::min(src.size(), dst_size - 1);
  std::memcpy(dst, src.data(), n);
  dst[n] = '\0';
}

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

std::uint64_t NowMicros() {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count());
}

}  // namespace

const char* ToString(EventType type) {
  switch (type) {
    case EventType::kProcessStart:
      return "process_start";
    case EventType::kServerStart:
      return "server_start";
    case EventType::kServerStop:
      return "server_stop";
    case EventType::kCircuitBreakerOpen:
      return "circuit_breaker_open";
    case EventType::kCircuitBreakerHalfOpen:
      return "circuit_breaker_half_open";
    case EventType::kCircuitBreakerClosed:
      return "circuit_breaker_closed";
    case EventType::kPeerUp:
      return "peer_up";
    case EventType::kPeerDown:
      return "peer_down";
    case EventType::kRateLimitTriggered:
      return "rate_limit_triggered";
    case EventType::kCustom:
      return "custom";
  }
  return "unknown";
}

const char* ToString(EventSeverity severity) {
  switch (severity) {
    case EventSeverity::kInfo:
      return "info";
    case EventSeverity::kWarn:
      return "warn";
    case EventSeverity::kError:
      return "error";
  }
  return "unknown";
}

EventJournal::EventJournal() { ring_.resize(kDefaultCapacity); }

EventJournal& EventJournal::Instance() {
  static EventJournal instance;
  return instance;
}

void EventJournal::Emit(EventType type, EventSeverity severity,
                        std::string_view subject, std::string_view message) {
  AppEvent ev;
  ev.timestamp_us = NowMicros();
  ev.type = type;
  ev.severity = severity;
  CopyTruncated(ev.subject, sizeof(ev.subject), subject);
  CopyTruncated(ev.message, sizeof(ev.message), message);

  emitted_total_.fetch_add(1, std::memory_order_relaxed);
  std::lock_guard lk{mu_};
  ring_[next_ % ring_.size()] = ev;
  ++next_;
}

std::vector<AppEvent> EventJournal::Snapshot() const {
  std::lock_guard lk{mu_};
  const std::size_t count = std::min(next_, ring_.size());
  std::vector<AppEvent> out;
  out.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    out.push_back(ring_[(next_ - 1 - i) % ring_.size()]);
  }
  return out;
}

std::string EventJournal::RenderJson() const {
  const std::vector<AppEvent> events = Snapshot();
  std::string out;
  out.reserve(events.size() * 192 + 64);
  out += "{\"emitted_total\":";
  out += std::to_string(emitted_total());
  out += ",\"events\":[";
  bool first = true;
  for (const AppEvent& ev : events) {
    if (!first) out += ',';
    first = false;
    out += "{\"timestamp_us\":";
    out += std::to_string(ev.timestamp_us);
    out += ",\"type\":\"";
    out += ToString(ev.type);
    out += "\",\"severity\":\"";
    out += ToString(ev.severity);
    out += "\",\"subject\":\"";
    AppendJsonEscaped(out, ev.subject);
    out += "\",\"message\":\"";
    AppendJsonEscaped(out, ev.message);
    out += "\"}";
  }
  out += "]}";
  return out;
}

}  // namespace runtime::events
