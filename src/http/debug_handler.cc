// Copyright (c) 2026 Arsenova
// SPDX-License-Identifier: MIT
#include "runtime/http/debug_handler.h"

#include <unistd.h>

#include <string>

#include "runtime/events/event_journal.h"
#include "runtime/http/http_request.h"
#include "runtime/http/http_response.h"
#include "runtime/time/timestamp.h"
#include "runtime/trace/trace_context.h"
#include "runtime/trace/trace_recorder.h"

namespace runtime::http {

namespace {

void FillJsonResponse(HttpResponse& resp, std::string body) {
  resp.set_status_code(StatusCode::Ok);
  resp.set_content_type("application/json; charset=utf-8");
  resp.set_body(std::move(body));
}

// 进程内首次构造任一 debug handler 的时刻, 作为 uptime 基准.
runtime::time::Timestamp ProcessBaseline() {
  static const runtime::time::Timestamp baseline =
      runtime::time::Timestamp::Now();
  return baseline;
}

}  // namespace

Handler MakeTracesDebugHandler() {
  (void)ProcessBaseline();
  return [](const HttpRequest&, HttpResponse& resp) {
    FillJsonResponse(resp, trace::TraceRecorder::Instance().RenderJson());
  };
}

Handler MakeEventsDebugHandler() {
  (void)ProcessBaseline();
  return [](const HttpRequest&, HttpResponse& resp) {
    FillJsonResponse(resp, events::EventJournal::Instance().RenderJson());
  };
}

Handler MakeStatusDebugHandler() {
  (void)ProcessBaseline();
  return [](const HttpRequest&, HttpResponse& resp) {
    const auto now = runtime::time::Timestamp::Now();
    const double uptime_s =
        runtime::time::TimeDifference(now, ProcessBaseline());

    std::string body;
    body.reserve(512);
    body += "{\"pid\":";
    body += std::to_string(::getpid());
    body += ",\"uptime_seconds\":";
    body += std::to_string(uptime_s);
    body += ",\"now_us\":";
    body += std::to_string(now.MicrosecondsSinceEpoch());
    body += ",\"traces\":{\"recorded_total\":";
    body += std::to_string(trace::TraceRecorder::Instance().recorded_total());
    body += ",\"ring_capacity\":";
    body += std::to_string(trace::TraceRecorder::kDefaultCapacity);
    body += ",\"sample_ratio\":";
    body += std::to_string(trace::TraceRecorder::Instance().sample_ratio());
    body += "},\"events\":{\"emitted_total\":";
    body += std::to_string(events::EventJournal::Instance().emitted_total());
    body += ",\"ring_capacity\":";
    body += std::to_string(events::EventJournal::kDefaultCapacity);
    body += "},\"current_trace\":\"";
    // 处于 ScopedTrace 作用域内时回显当前 trace_id, 便于 curl 验证关联
    if (trace::CurrentTrace().Valid()) {
      body += trace::CurrentTrace().TraceIdHex();
    }
    body += "\"}";
    FillJsonResponse(resp, std::move(body));
  };
}

}  // namespace runtime::http
