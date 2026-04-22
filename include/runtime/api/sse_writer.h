#pragma once

#include "runtime/net/tcp_connection.h"

#include <string>
#include <string_view>

namespace runtime::api {

// Writes Server-Sent Events (SSE) frames to a persistent TcpConnection.
// All Write* methods must be called from the connection's IO thread.
//
// Wire format per chunk:
//   data: <json>\n\n
// OpenAI streaming terminator:
//   data: [DONE]\n\n
class SseWriter {
public:
    static void WriteToken(const runtime::net::TcpConnection::TcpConnectionPtr& conn,
                           std::string_view token,
                           std::string_view model = "");

    static void WriteDone(const runtime::net::TcpConnection::TcpConnectionPtr& conn);

    static void WriteError(const runtime::net::TcpConnection::TcpConnectionPtr& conn,
                           std::string_view message);

    // Returns raw HTTP/1.1 SSE response header block.
    static std::string BuildHeaders(int status_code = 200);

    static std::string FormatChunk(std::string_view token, std::string_view model);
    static std::string FormatDone();
};

}  // namespace runtime::api
