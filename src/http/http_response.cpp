#include "runtime/http/http_response.h"

#include <sstream>

namespace runtime::http {

namespace {

const char* ReasonPhrase(HttpResponse::StatusCode code) {
    switch (code) {
    case HttpResponse::StatusCode::_200_Ok:
        return "OK";
    case HttpResponse::StatusCode::_400_BadRequest:
        return "Bad Request";
    case HttpResponse::StatusCode::_403_Forbidden:
        return "Forbidden";
    case HttpResponse::StatusCode::_404_NotFound:
        return "Not Found";
    case HttpResponse::StatusCode::_405_MethodNotAllowed:
        return "Method Not Allowed";
    case HttpResponse::StatusCode::_408_RequestTimeout:
        return "Request Timeout";
    case HttpResponse::StatusCode::_500_InternalServerError:
        return "Internal Server Error";
    case HttpResponse::StatusCode::Unknown:
    default:
        return "Unknown";
    }
}

}  // namespace

HttpResponse::HttpResponse(bool close)
    : close_connection_(close) {}

void HttpResponse::SetStatusCode(StatusCode code) {
    status_code_ = code;
}

void HttpResponse::SetStatusMessage(std::string message) {
    status_message_ = std::move(message);
}

void HttpResponse::SetCloseConnection(bool close) {
    close_connection_ = close;
}

bool HttpResponse::CloseConnection() const {
    return close_connection_;
}

void HttpResponse::SetContentType(std::string content_type) {
    headers_["Content-Type"] = std::move(content_type);
}

void HttpResponse::SetConnectionType(std::string content_type) {
    SetContentType(std::move(content_type));
}

void HttpResponse::SetBody(std::string body) {
    body_ = std::move(body);
}

void HttpResponse::AddHeader(std::string key, std::string value) {
    headers_[std::move(key)] = std::move(value);
}

std::string HttpResponse::ToString() const {
    std::ostringstream oss;
    const std::string& reason =
        status_message_.empty() ? std::string(ReasonPhrase(status_code_)) : status_message_;

    oss << "HTTP/1.1 " << static_cast<int>(status_code_) << ' ' << reason << "\r\n";
    oss << "Connection: " << (close_connection_ ? "close" : "keep-alive") << "\r\n";

    for (const auto& [key, value] : headers_) {
        oss << key << ": " << value << "\r\n";
    }

    oss << "Content-Length: " << body_.size() << "\r\n";
    oss << "\r\n";
    oss << body_;
    return oss.str();
}

void HttpResponse::Reset() {
    status_code_ = StatusCode::Unknown;
    status_message_.clear();
    close_connection_ = true;
    headers_.clear();
    body_.clear();
}

}  // namespace runtime::http
