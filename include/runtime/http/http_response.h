#pragma once

#include <string>
#include <unordered_map>
namespace runtime::http {

class HttpResponse {
public:
    enum class StatusCode {
        Unknown = 0,
        _200_Ok = 200,
        _400_BadRequest = 400,
        _404_NotFound = 404,
        _500_InternalServerError = 500
    };

    explicit HttpResponse(bool close = true);

    void SetStatusCode(StatusCode code);
    void SetStatusMessage(std::string message);

    void SetCloseConnection(bool close);
    bool CloseConnection() const;

    void SetContentType(std::string content_type);
    void SetBody(std::string body);
    void AddHeader(std::string key, std::string value);

    std::string ToString() const;
    void Reset();

private:
    StatusCode status_code_{StatusCode::Unknown};
    std::string status_message_;
    bool close_connection_{true};
    std::unordered_map<std::string, std::string> headers_;
    std::string body_;
};
}