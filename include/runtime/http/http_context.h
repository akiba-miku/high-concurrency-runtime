#pragma once

#include "runtime/http/http_request.h"
#include "runtime/time/timestamp.h"
#include <string>

namespace runtime::http{

class HttpContext{
public:
    enum class ParseState{
        ExpectRequestLine,
        ExpectHeaders,
        ExpectBody,
        GotAll
    };

    bool ParseRequest(const std::string &data, runtime::time::Timestamp receive_time);
    bool GotAll() const;
    bool HasBufferedData() const;
    void Reset();

    const HttpRequest &Request() const;
    HttpRequest &MutableRequest();

private:
    bool ProcessRequestLine(const std::string &line);
    bool ProcessHeaderLine(const std::string &line);
private:
    ParseState state_{ParseState::ExpectRequestLine};
    HttpRequest request_;
    std::size_t body_bytes_expected_{0};
    std::string pending_;
};
}
