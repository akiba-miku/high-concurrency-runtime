// Copyright (c) 2026 Arsenova
// SPDX-License-Identifier: MIT
//
// trace 核心 smoke test: traceparent 编解码、ID 生成、ScopedTrace、
// TraceRecorder 环形缓冲. 无 GTest 依赖, 始终编译.
#include <cassert>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>

#include "runtime/trace/trace_context.h"
#include "runtime/trace/trace_recorder.h"

namespace t = runtime::trace;

namespace {

void TestParseRoundTrip() {
  const std::string header =
      "00-0af7651916cd43dd8448eb211c80319c-b7ad6b7169203331-01";
  auto ctx = t::ParseTraceparent(header);
  assert(ctx.has_value());
  assert(ctx->trace_id_hi == 0x0af7651916cd43ddULL);
  assert(ctx->trace_id_lo == 0x8448eb211c80319cULL);
  assert(ctx->span_id == 0xb7ad6b7169203331ULL);
  assert(ctx->flags == 0x01);
  assert(ctx->Sampled());
  assert(ctx->Valid());
  assert(ctx->ToTraceparent() == header);
  assert(ctx->TraceIdHex() == "0af7651916cd43dd8448eb211c80319c");
  assert(ctx->SpanIdHex() == "b7ad6b7169203331");
}

void TestParseRejectsBadInput() {
  // 长度不对
  assert(!t::ParseTraceparent(""));
  assert(!t::ParseTraceparent("00-abc"));
  assert(!t::ParseTraceparent(
      "00-0af7651916cd43dd8448eb211c80319c-b7ad6b7169203331-012"));
  // 版本非 00 (含保留版本 ff)
  assert(!t::ParseTraceparent(
      "01-0af7651916cd43dd8448eb211c80319c-b7ad6b7169203331-01"));
  assert(!t::ParseTraceparent(
      "ff-0af7651916cd43dd8448eb211c80319c-b7ad6b7169203331-01"));
  // 分隔符错位
  assert(!t::ParseTraceparent(
      "00_0af7651916cd43dd8448eb211c80319c-b7ad6b7169203331-01"));
  // 非 hex 字符
  assert(!t::ParseTraceparent(
      "00-0af7651916cd43dd8448eb211c80319g-b7ad6b7169203331-01"));
  // 全零 trace_id / span_id
  assert(!t::ParseTraceparent(
      "00-00000000000000000000000000000000-b7ad6b7169203331-01"));
  assert(!t::ParseTraceparent(
      "00-0af7651916cd43dd8448eb211c80319c-0000000000000000-01"));
}

void TestRootAndChild() {
  t::TraceContext root = t::NewRootContext();
  assert(root.Valid());
  assert(root.Sampled());
  assert(root.parent_span_id == 0);

  t::TraceContext child = t::NewChildContext(root);
  assert(child.Valid());
  assert(child.trace_id_hi == root.trace_id_hi);
  assert(child.trace_id_lo == root.trace_id_lo);
  assert(child.span_id != root.span_id);
  assert(child.parent_span_id == root.span_id);
  assert(child.flags == root.flags);
}

void TestIdUniqueness() {
  std::set<std::uint64_t> seen;
  for (int i = 0; i < 10000; ++i) {
    std::uint64_t v = t::RandomNonZeroU64();
    assert(v != 0);
    seen.insert(v);
  }
  // 64 位随机数 1 万次抽样, 撞一次的概率可忽略
  assert(seen.size() == 10000);
}

void TestScopedTraceNesting() {
  assert(!t::CurrentTrace().Valid());
  t::TraceContext outer = t::NewRootContext();
  {
    t::ScopedTrace s1{outer};
    assert(t::CurrentTrace().span_id == outer.span_id);
    t::TraceContext inner = t::NewChildContext(outer);
    {
      t::ScopedTrace s2{inner};
      assert(t::CurrentTrace().span_id == inner.span_id);
    }
    assert(t::CurrentTrace().span_id == outer.span_id);
  }
  assert(!t::CurrentTrace().Valid());
}

void TestRecorderRingWrap() {
  auto& rec = t::TraceRecorder::Instance();
  const std::size_t cap = t::TraceRecorder::kDefaultCapacity;
  for (std::size_t i = 0; i < cap + 10; ++i) {
    t::SpanRecord span;
    span.ctx = t::NewRootContext();
    span.kind = t::SpanKind::kServer;
    span.start_us = 1000 + i;
    span.duration_us = i;
    span.status_code = 200;
    span.set_name("GET /ring");
    rec.Record(span);
  }
  assert(rec.recorded_total() == cap + 10);
  auto snap = rec.Snapshot();
  assert(snap.size() == cap);
  // 最新优先: 第一条是最后写入的那条
  assert(snap.front().duration_us == cap + 9);
  assert(snap.back().duration_us == 10);  // 最旧的 10 条已被覆盖
}

void TestRecorderTruncationAndJson() {
  auto& rec = t::TraceRecorder::Instance();
  t::SpanRecord span;
  span.ctx = t::NewRootContext();
  span.kind = t::SpanKind::kClient;
  span.status_code = 502;
  const std::string long_name(100, 'n');
  span.set_name(long_name);
  span.set_attr("peer\"quoted\\attr");
  assert(std::strlen(span.name) == sizeof(span.name) - 1);
  rec.Record(span);

  const std::string json = rec.RenderJson();
  assert(!json.empty());
  assert(json.front() == '{' && json.back() == '}');
  assert(json.find("\"spans\":[") != std::string::npos);
  assert(json.find("\"kind\":\"client\"") != std::string::npos);
  assert(json.find("\"status\":502") != std::string::npos);
  // 引号与反斜杠被转义, 原始裸引号不应出现在 attr 值里
  assert(json.find("peer\\\"quoted\\\\attr") != std::string::npos);
}

void TestSampleRatioZero() {
  auto& rec = t::TraceRecorder::Instance();
  const std::uint64_t before = rec.recorded_total();
  rec.set_sample_ratio(0.0);
  t::SpanRecord span;
  span.ctx = t::NewRootContext();
  for (int i = 0; i < 100; ++i) rec.Record(span);
  assert(rec.recorded_total() == before);
  rec.set_sample_ratio(1.0);
  rec.Record(span);
  assert(rec.recorded_total() == before + 1);
}

}  // namespace

int main() {
  TestParseRoundTrip();
  TestParseRejectsBadInput();
  TestRootAndChild();
  TestIdUniqueness();
  TestScopedTraceNesting();
  TestRecorderRingWrap();
  TestRecorderTruncationAndJson();
  TestSampleRatioZero();
  std::puts("[trace_smoke] ok");
  return 0;
}
