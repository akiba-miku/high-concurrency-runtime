#include "runtime/http/http_context.h"
#include "runtime/http/http_request.h"
#include "runtime/http/http_response.h"

#include <iostream>
#include <string>

namespace {

bool Expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "[FAIL] " << message << '\n';
        return false;
    }
    return true;
}

bool TestParsesHttp11KeepAliveRequest() {
    runtime::http::HttpContext context;
    const std::string request =
        "GET /api/health?verbose=1 HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: keep-alive\r\n"
        "X-Trace-Id: trace-123\r\n"
        "\r\n";

    bool ok = Expect(context.ParseRequest(request, runtime::time::Timestamp::Now()),
                     "parser should accept a valid HTTP/1.1 request");
    ok &= Expect(context.GotAll(), "parser should complete the request");

    const runtime::http::HttpRequest& parsed = context.Request();
    ok &= Expect(parsed.GetMethod() == runtime::http::HttpRequest::Method::Get,
                 "request method should be GET");
    ok &= Expect(parsed.Path() == "/api/health", "path should be parsed");
    ok &= Expect(parsed.Query() == "verbose=1", "query string should be parsed");
    ok &= Expect(parsed.KeepAlive(), "HTTP/1.1 keep-alive should be enabled");
    ok &= Expect(parsed.GetHeader("x-trace-id") == "trace-123",
                 "trace id header should be accessible case-insensitively");
    return ok;
}

bool TestBuildsHttpResponse() {
    runtime::http::HttpResponse response(false);
    response.SetStatusCode(runtime::http::HttpResponse::StatusCode::_200_Ok);
    response.SetContentType("application/json; charset=utf-8");
    response.AddHeader("X-Trace-Id", "trace-abc");
    response.SetBody("{\"ok\":true}");

    const std::string encoded = response.ToString();
    bool ok = Expect(encoded.find("HTTP/1.1 200 OK\r\n") == 0,
                     "response should start with a status line");
    ok &= Expect(encoded.find("Connection: keep-alive\r\n") != std::string::npos,
                 "response should encode keep-alive");
    ok &= Expect(encoded.find("X-Trace-Id: trace-abc\r\n") != std::string::npos,
                 "response should include trace id");
    ok &= Expect(encoded.find("Content-Length: 11\r\n") != std::string::npos,
                 "response should include content length");
    return ok;
}

}  // namespace

int main() {
    const bool ok = TestParsesHttp11KeepAliveRequest() && TestBuildsHttpResponse();
    if (!ok) {
        return 1;
    }

    std::cout << "[PASS] http_smoke_test\n";
    return 0;
}
