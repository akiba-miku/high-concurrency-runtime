// Copyright (c) 2026 Arsenova
// SPDX-License-Identifier: MIT
//
// /debug/* handler smoke test: 直接调用三个 Handler 工厂的产物,
// 断言响应码 / content-type / body 内容与 foundation 单例联动.
#include <cassert>
#include <cstdio>
#include <string>

#include "runtime/events/event_journal.h"
#include "runtime/http/debug_handler.h"
#include "runtime/http/http_request.h"
#include "runtime/http/http_response.h"
#include "runtime/trace/trace_context.h"
#include "runtime/trace/trace_recorder.h"

namespace h = runtime::http;
namespace t = runtime::trace;
namespace e = runtime::events;

namespace {

// 调用一个 handler 并返回填好的响应.
h::HttpResponse Invoke(const h::Handler& handler) {
  h::HttpRequest req;
  h::HttpResponse resp{/*close_connection=*/false};
  handler(req, resp);
  return resp;
}

void TestTracesEndpoint() {
  t::SpanRecord span;
  span.ctx = t::NewRootContext();
  span.kind = t::SpanKind::kServer;
  span.status_code = 200;
  span.set_name("GET /debug-smoke-marker");
  t::TraceRecorder::Instance().Record(span);

  h::HttpResponse resp = Invoke(h::MakeTracesDebugHandler());
  assert(resp.status_code() == h::StatusCode::Ok);
  assert(resp.content_type().find("application/json") != std::string::npos);
  assert(resp.body().find("\"spans\":[") != std::string::npos);
  assert(resp.body().find("GET /debug-smoke-marker") != std::string::npos);
  assert(resp.body().find(span.ctx.TraceIdHex()) != std::string::npos);
}

void TestEventsEndpoint() {
  e::EventJournal::Instance().Emit(e::EventType::kServerStart,
                                   e::EventSeverity::kInfo,
                                   "debug-smoke", "events endpoint marker");
  h::HttpResponse resp = Invoke(h::MakeEventsDebugHandler());
  assert(resp.status_code() == h::StatusCode::Ok);
  assert(resp.content_type().find("application/json") != std::string::npos);
  assert(resp.body().find("\"events\":[") != std::string::npos);
  assert(resp.body().find("events endpoint marker") != std::string::npos);
  assert(resp.body().find("\"type\":\"server_start\"") != std::string::npos);
}

void TestStatusEndpoint() {
  h::HttpResponse resp = Invoke(h::MakeStatusDebugHandler());
  assert(resp.status_code() == h::StatusCode::Ok);
  const std::string& body = resp.body();
  assert(body.find("\"pid\":") != std::string::npos);
  assert(body.find("\"uptime_seconds\":") != std::string::npos);
  assert(body.find("\"traces\":{") != std::string::npos);
  assert(body.find("\"events\":{") != std::string::npos);
  // 无 ScopedTrace 作用域时 current_trace 为空串
  assert(body.find("\"current_trace\":\"\"") != std::string::npos);
}

void TestStatusEchoesCurrentTrace() {
  const t::TraceContext ctx = t::NewRootContext();
  t::ScopedTrace scoped{ctx};
  h::HttpResponse resp = Invoke(h::MakeStatusDebugHandler());
  assert(resp.body().find(ctx.TraceIdHex()) != std::string::npos);
}

}  // namespace

int main() {
  TestTracesEndpoint();
  TestEventsEndpoint();
  TestStatusEndpoint();
  TestStatusEchoesCurrentTrace();
  std::puts("[debug_handler_smoke] ok");
  return 0;
}
