#include "runtime/http/http_request.h"
#include "runtime/http/http_types.h"

#include <cctype>

namespace runtime::http {

namespace {

std::string ToLower(std::string_view sv) {
  std::string out{sv};
  for (char& c : out) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return out;
}

}  // namespace

void HttpRequest::AddHeader(std::string_view field, std::string_view value) {
  headers_.emplace(ToLower(field), std::string(value));
}

std::string_view HttpRequest::GetHeader(std::string_view field) const {
  const auto it = headers_.find(ToLower(field));
  if (it == headers_.end()) {
    return {};
  }
  return it->second;
}

bool HttpRequest::KeepAlive() const {
  const auto conn = GetHeader("connection");
  if (version_ == Version::Http11) {
    // HTTP/1.1 keeps connections alive by default unless the client sends
    // Connection: close.
    return conn != "close";
  }
  return conn == "keep-alive";
}

std::string HttpRequest::SerializeToString() const {
  std::string out;
  out.reserve(256);
  out += MethodToString(method_);
  out += ' ';
  out += path_;
  if (!query_.empty()) {
    out += '?';
    out += query_;
  }
  out += ' ';
  out += (version_ == Version::Http11) ? "HTTP/1.1" : "HTTP/1.0";
  out += "\r\n";
  for (const auto& [k, v] : headers_) {
    out += k;
    out += ": ";
    out += v;
    out += "\r\n";
  }
  out += "\r\n";
  out += body_;
  return out;
}

void HttpRequest::Reset() {
  method_ = Method::Invalid;
  version_ = Version::Unknown;
  path_params_.clear();
  path_.clear();
  query_.clear();
  body_.clear();
  headers_.clear();
  receive_time_ = {};
}

}  // namespace runtime::http
