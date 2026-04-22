#include "runtime/api/sse_writer.h"

#include <string>

namespace runtime::api {

std::string SseWriter::FormatChunk(std::string_view token,
                                   std::string_view model) {
    std::string escaped;
    escaped.reserve(token.size());
    for (char c : token) {
        if (c == '"')       escaped += "\\\"";
        else if (c == '\\') escaped += "\\\\";
        else if (c == '\n') escaped += "\\n";
        else if (c == '\r') escaped += "\\r";
        else                escaped += c;
    }

    return "data: {\"object\":\"chat.completion.chunk\",\"model\":\""
           + std::string(model)
           + "\",\"choices\":[{\"delta\":{\"content\":\""
           + escaped
           + "\"},\"index\":0,\"finish_reason\":null}]}\n\n";
}

std::string SseWriter::FormatDone() {
    return "data: [DONE]\n\n";
}

std::string SseWriter::BuildHeaders(int status_code) {
    std::string reason = (status_code == 200) ? "OK" : "Error";
    return "HTTP/1.1 " + std::to_string(status_code) + " " + reason + "\r\n"
           "Content-Type: text/event-stream\r\n"
           "Cache-Control: no-cache\r\n"
           "Connection: keep-alive\r\n"
           "Transfer-Encoding: chunked\r\n"
           "\r\n";
}

void SseWriter::WriteToken(const runtime::net::TcpConnection::TcpConnectionPtr& conn,
                           std::string_view token,
                           std::string_view model) {
    if (!conn || !conn->Connected()) return;
    conn->Send(FormatChunk(token, model));
}

void SseWriter::WriteDone(const runtime::net::TcpConnection::TcpConnectionPtr& conn) {
    if (!conn || !conn->Connected()) return;
    conn->Send(FormatDone());
}

void SseWriter::WriteError(const runtime::net::TcpConnection::TcpConnectionPtr& conn,
                           std::string_view message) {
    if (!conn || !conn->Connected()) return;
    std::string escaped;
    for (char c : message) {
        if (c == '"')       escaped += "\\\"";
        else if (c == '\\') escaped += "\\\\";
        else                escaped += c;
    }
    conn->Send("data: {\"error\":\"" + escaped + "\"}\n\n");
}

}  // namespace runtime::api
