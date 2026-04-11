#pragma once

#include "runtime/time/timestamp.h"
#include <string>
#include <unordered_map>
namespace runtime::http {
/**
 * 负责表示一个已解析的请求
 */
class HttpRequest {
public:
    enum class Method {
        Invaild,
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

    void SetQuery(std::string body);
    const std::string &Query() const;

    void AddHeader(std::string field, std::string value);
    std::string GetHeader(const std::string &field) const;
    const std::unordered_map<std::string, std::string> &Headers() const;

    void SetReciveTime(runtime::time::Timestamp ts);
    runtime::time::Timestamp ReciveTime() const;

    void Reset();

private:
    Method method_{Method::Invaild};
    Version version_{Version::Unknown};
    std::string path_;
    std::string query_;
    std::unordered_map<std::string ,std::string> headers_;
    runtime::time::Timestamp receive_time_;
};
}