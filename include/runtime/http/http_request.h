#pragma once

#include "runtime/time/timestamp.h"

#include <string>
#include <unordered_map>

namespace runtime::http {

class HttpRequest {
public:
    enum class Method {
        Invalid,
        Get,
        Post,
        Put,
        Delete,
        Head
    };

    enum class Version {
        Unknown,
        Http10,
        Http11,
    };

    void SetMethod(Method method);
    Method GetMethod() const;

    void SetVersion(Version version);
    Version GetVersion() const;

    void SetPath(std::string path);
    const std::string &Path() const;

    void SetBody(std::string body);
    const std::string &Body() const;

    void SetQuery(std::string query);
    const std::string &Query() const;
    const std::string &Qurey() const;

    void AddHeader(std::string field, std::string value);
    std::string GetHeader(const std::string &field) const;
    const std::unordered_map<std::string, std::string> &Headers() const;

    void SetReceiveTime(runtime::time::Timestamp ts);
    void SetReciveTime(runtime::time::Timestamp ts);
    runtime::time::Timestamp ReceiveTime() const;
    runtime::time::Timestamp ReciveTime() const;

    bool KeepAlive() const;

    void Reset();

private:
    Method method_{Method::Invalid};
    Version version_{Version::Unknown};
    std::string path_;
    std::string query_;
    std::string body_;
    std::unordered_map<std::string ,std::string> headers_;
    runtime::time::Timestamp receive_time_;
};
}  // namespace runtime::http
