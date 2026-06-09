// Copyright (c) 2026 Arsenova
// SPDX-License-Identifier: MIT
#pragma once

#include "runtime/http/router.h"

// /debug/* 自省端点 —— 可观测性模块的 HTTP 出口.
// 提供 Handler 工厂 (不直接绑定到某个 server), HttpServer 与 GatewayServer
// 都可以用各自的路由注册方式接入, 不引入新耦合:
//
//   server.Get("/debug/traces", runtime::http::MakeTracesDebugHandler());
//   server.Get("/debug/events", runtime::http::MakeEventsDebugHandler());
//   server.Get("/debug/status", runtime::http::MakeStatusDebugHandler());
//
// 三个端点都返回 application/json, 数据来自 foundation 层的
// TraceRecorder / EventJournal 进程级单例, 读路径只做一次环快照拷贝.
namespace runtime::http {

// GET /debug/traces — 最近完成的 span (TraceRecorder 环内容, 最新优先).
[[nodiscard]] Handler MakeTracesDebugHandler();

// GET /debug/events — 应用事件日志 (EventJournal 环内容, 最新优先).
[[nodiscard]] Handler MakeEventsDebugHandler();

// GET /debug/status — 进程自省: uptime、pid、trace/event 累计量等.
// uptime 起点 = 本进程内首次调用任一 Make*DebugHandler() 的时刻.
[[nodiscard]] Handler MakeStatusDebugHandler();

}  // namespace runtime::http
