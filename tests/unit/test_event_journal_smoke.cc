// Copyright (c) 2026 Arsenova
// SPDX-License-Identifier: MIT
//
// EventJournal smoke test: 环回绕、截断、并发 Emit、JSON 渲染.
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "runtime/events/event_journal.h"

namespace e = runtime::events;

namespace {

void TestEmitAndSnapshotOrder() {
  auto& journal = e::EventJournal::Instance();
  journal.Emit(e::EventType::kProcessStart, e::EventSeverity::kInfo, "proc",
               "first");
  journal.Emit(e::EventType::kServerStart, e::EventSeverity::kInfo, "gw",
               "second");
  auto snap = journal.Snapshot();
  assert(snap.size() >= 2);
  // 最新优先
  assert(snap[0].type == e::EventType::kServerStart);
  assert(std::strcmp(snap[0].subject, "gw") == 0);
  assert(snap[1].type == e::EventType::kProcessStart);
  assert(snap[0].timestamp_us >= snap[1].timestamp_us);
}

void TestTruncation() {
  auto& journal = e::EventJournal::Instance();
  const std::string long_subject(100, 's');
  const std::string long_message(200, 'm');
  journal.Emit(e::EventType::kCustom, e::EventSeverity::kWarn, long_subject,
               long_message);
  auto snap = journal.Snapshot();
  const e::AppEvent& ev = snap.front();
  assert(std::strlen(ev.subject) == sizeof(ev.subject) - 1);
  assert(std::strlen(ev.message) == sizeof(ev.message) - 1);
  assert(ev.subject[0] == 's' && ev.message[0] == 'm');
}

void TestRingWrap() {
  auto& journal = e::EventJournal::Instance();
  const std::uint64_t before = journal.emitted_total();
  const std::size_t cap = e::EventJournal::kDefaultCapacity;
  for (std::size_t i = 0; i < cap + 20; ++i) {
    journal.Emit(e::EventType::kPeerDown, e::EventSeverity::kError, "peer",
                 std::to_string(i));
  }
  assert(journal.emitted_total() == before + cap + 20);
  auto snap = journal.Snapshot();
  assert(snap.size() == cap);
  assert(std::strcmp(snap.front().message,
                     std::to_string(cap + 19).c_str()) == 0);
}

void TestConcurrentEmit() {
  auto& journal = e::EventJournal::Instance();
  const std::uint64_t before = journal.emitted_total();
  constexpr int kThreads = 4;
  constexpr int kPerThread = 1000;
  {
    std::vector<std::jthread> workers;
    for (int t = 0; t < kThreads; ++t) {
      workers.emplace_back([&journal, t] {
        for (int i = 0; i < kPerThread; ++i) {
          journal.Emit(e::EventType::kRateLimitTriggered,
                       e::EventSeverity::kWarn, "global",
                       "thread " + std::to_string(t));
        }
      });
    }
  }
  assert(journal.emitted_total() == before + kThreads * kPerThread);
  assert(journal.Snapshot().size() == e::EventJournal::kDefaultCapacity);
}

void TestRenderJson() {
  auto& journal = e::EventJournal::Instance();
  journal.Emit(e::EventType::kCircuitBreakerOpen, e::EventSeverity::kError,
               "upstream_a", "fail \"rate\" > 50%\\");
  const std::string json = journal.RenderJson();
  assert(!json.empty());
  assert(json.front() == '{' && json.back() == '}');
  assert(json.find("\"events\":[") != std::string::npos);
  assert(json.find("\"type\":\"circuit_breaker_open\"") != std::string::npos);
  assert(json.find("\"severity\":\"error\"") != std::string::npos);
  // 转义检查: 裸引号不应出现在值内
  assert(json.find("fail \\\"rate\\\" > 50%\\\\") != std::string::npos);
}

void TestToString() {
  assert(std::strcmp(e::ToString(e::EventType::kPeerUp), "peer_up") == 0);
  assert(std::strcmp(e::ToString(e::EventSeverity::kInfo), "info") == 0);
}

}  // namespace

int main() {
  TestEmitAndSnapshotOrder();
  TestTruncation();
  TestRingWrap();
  TestConcurrentEmit();
  TestRenderJson();
  TestToString();
  std::puts("[event_journal_smoke] ok");
  return 0;
}
