#include "runtime/http/http_context.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <string_view>

namespace runtime::http {

namespace {

std::string Trim(std::string_view value) {
    std::size_t begin = 0;
    while (begin < value.size() &&
           std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
        ++begin;
    }

    std::size_t end = value.size();
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }

    return std::string(value.substr(begin, end - begin));
}

HttpRequest::Method ParseMethod(std::string_view method) {
    if (method == "GET") {
        return HttpRequest::Method::Get;
    }
    if (method == "POST") {
        return HttpRequest::Method::Post;
    }
    if (method == "PUT") {
        return HttpRequest::Method::Put;
    }
    if (method == "DELETE") {
        return HttpRequest::Method::Delete;
    }
    if (method == "HEAD") {
        return HttpRequest::Method::Head;
    }
    return HttpRequest::Method::Invalid;
}

}  // namespace

bool HttpContext::ParseRequest(const std::string& data,
                               runtime::time::Timestamp receive_time) {
    pending_.append(data);

    while (state_ != ParseState::GotAll) {
        if (state_ == ParseState::ExpectRequestLine) {
            const std::size_t line_end = pending_.find("\r\n");
            if (line_end == std::string::npos) {
                return true;
            }

            const std::string line = pending_.substr(0, line_end);
            pending_.erase(0, line_end + 2);
            if (!ProcessRequestLine(line)) {
                return false;
            }
            state_ = ParseState::ExpectHeaders;
            continue;
        }

        if (state_ == ParseState::ExpectHeaders) {
            const std::size_t line_end = pending_.find("\r\n");
            if (line_end == std::string::npos) {
                return true;
            }

            const std::string line = pending_.substr(0, line_end);
            pending_.erase(0, line_end + 2);

            if (line.empty()) {
                if (body_bytes_expected_ == 0) {
                    request_.SetReceiveTime(receive_time);
                    state_ = ParseState::GotAll;
                    return true;
                }
                state_ = ParseState::ExpectBody;
                continue;
            }

            if (!ProcessHeaderLine(line)) {
                return false;
            }
            continue;
        }

        if (state_ == ParseState::ExpectBody) {
            if (pending_.size() < body_bytes_expected_) {
                return true;
            }

            request_.SetBody(pending_.substr(0, body_bytes_expected_));
            pending_.erase(0, body_bytes_expected_);
            request_.SetReceiveTime(receive_time);
            state_ = ParseState::GotAll;
        }
    }

    return true;
}

bool HttpContext::GotAll() const {
    return state_ == ParseState::GotAll;
}

bool HttpContext::HasBufferedData() const {
    return !pending_.empty();
}

void HttpContext::Reset() {
    state_ = ParseState::ExpectRequestLine;
    request_.Reset();
    body_bytes_expected_ = 0;
}

const HttpRequest& HttpContext::Request() const {
    return request_;
}

HttpRequest& HttpContext::MutableRequest() {
    return request_;
}

bool HttpContext::ProcessRequestLine(const std::string& line) {
    const std::size_t method_end = line.find(' ');
    if (method_end == std::string::npos) {
        return false;
    }
    const std::size_t uri_end = line.find(' ', method_end + 1);
    if (uri_end == std::string::npos) {
        return false;
    }

    const std::string_view method_view(line.data(), method_end);
    const std::string_view uri_view(line.data() + method_end + 1,
                                    uri_end - method_end - 1);
    const std::string_view version_view(line.data() + uri_end + 1,
                                        line.size() - uri_end - 1);

    const HttpRequest::Method method = ParseMethod(method_view);
    if (method == HttpRequest::Method::Invalid) {
        return false;
    }

    request_.SetMethod(method);
    if (version_view == "HTTP/1.1") {
        request_.SetVersion(HttpRequest::Version::Http11);
    } else if (version_view == "HTTP/1.0") {
        request_.SetVersion(HttpRequest::Version::Http10);
    } else {
        return false;
    }

    const std::size_t query_pos = uri_view.find('?');
    if (query_pos == std::string_view::npos) {
        request_.SetPath(std::string(uri_view));
        request_.SetQuery("");
    } else {
        request_.SetPath(std::string(uri_view.substr(0, query_pos)));
        request_.SetQuery(std::string(uri_view.substr(query_pos + 1)));
    }
    return true;
}

bool HttpContext::ProcessHeaderLine(const std::string& line) {
    const std::size_t colon = line.find(':');
    if (colon == std::string::npos) {
        return false;
    }

    const std::string field = Trim(std::string_view(line).substr(0, colon));
    const std::string value = Trim(std::string_view(line).substr(colon + 1));
    if (field.empty()) {
        return false;
    }

    request_.AddHeader(field, value);

    if (request_.GetHeader("content-length").empty()) {
        return true;
    }

    const std::string content_length = request_.GetHeader("content-length");
    std::size_t parsed_length = 0;
    const auto [ptr, ec] = std::from_chars(
        content_length.data(),
        content_length.data() + content_length.size(),
        parsed_length);
    if (ec != std::errc() || ptr != content_length.data() + content_length.size()) {
        return false;
    }
    body_bytes_expected_ = parsed_length;
    return true;
}

}  // namespace runtime::http
