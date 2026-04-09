#include "runtime/http/http_request.h"

#include <algorithm>
#include <cctype>

namespace runtime::http {

namespace {

std::string NormalizeHeaderKey(std::string key) {
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return key;
}

}  // namespace

void HttpRequest::SetMethod(Method method) {
    method_ = method;
}

HttpRequest::Method HttpRequest::GetMethod() const {
    return method_;
}

void HttpRequest::SetVersion(Version version) {
    version_ = version;
}

HttpRequest::Version HttpRequest::GetVersion() const {
    return version_;
}

void HttpRequest::SetPath(std::string path) {
    path_ = std::move(path);
}

const std::string& HttpRequest::Path() const {
    return path_;
}

void HttpRequest::SetBody(std::string body) {
    body_ = std::move(body);
}

const std::string& HttpRequest::Body() const {
    return body_;
}

void HttpRequest::SetQuery(std::string query) {
    query_ = std::move(query);
}

const std::string& HttpRequest::Query() const {
    return query_;
}

const std::string& HttpRequest::Qurey() const {
    return query_;
}

void HttpRequest::AddHeader(std::string field, std::string value) {
    headers_[NormalizeHeaderKey(std::move(field))] = std::move(value);
}

std::string HttpRequest::GetHeader(const std::string& field) const {
    const auto it = headers_.find(NormalizeHeaderKey(field));
    if (it == headers_.end()) {
        return "";
    }
    return it->second;
}

const std::unordered_map<std::string, std::string>& HttpRequest::Headers() const {
    return headers_;
}

void HttpRequest::SetReceiveTime(runtime::time::Timestamp ts) {
    receive_time_ = ts;
}

void HttpRequest::SetReciveTime(runtime::time::Timestamp ts) {
    SetReceiveTime(ts);
}

runtime::time::Timestamp HttpRequest::ReceiveTime() const {
    return receive_time_;
}

runtime::time::Timestamp HttpRequest::ReciveTime() const {
    return receive_time_;
}

bool HttpRequest::KeepAlive() const {
    std::string connection = GetHeader("connection");
    std::transform(connection.begin(), connection.end(), connection.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (version_ == Version::Http11) {
        return connection != "close";
    }
    return connection == "keep-alive";
}

void HttpRequest::Reset() {
    method_ = Method::Invalid;
    version_ = Version::Unknown;
    path_.clear();
    query_.clear();
    body_.clear();
    headers_.clear();
    receive_time_ = runtime::time::Timestamp::Invalid();
}

}  // namespace runtime::http
