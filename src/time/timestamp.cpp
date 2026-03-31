#include "runtime/time/timestamp.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace runtime::time {

// Timestamp 内部统一用微秒保存时间，兼顾日志展示和后续定时器精度。
Timestamp Timestamp::Now() {
    const auto now = std::chrono::system_clock::now();
    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch());
    return Timestamp(static_cast<std::uint64_t>(micros.count()));
}

Timestamp Timestamp::Invalid() {
    return Timestamp();
}

bool Timestamp::Valid() const {
    return microseconds_since_epoch_ > 0;
}

std::uint64_t Timestamp::MicrosecondsSinceEpoch() const {
    return microseconds_since_epoch_;
}

std::uint64_t Timestamp::MillisecondsSinceEpoch() const {
    return microseconds_since_epoch_ / 1000;
}

std::uint64_t Timestamp::SecondsSinceEpoch() const {
    return microseconds_since_epoch_ / 1000000;
}

std::string Timestamp::ToString() const {
    std::ostringstream oss;
    oss << SecondsSinceEpoch() << '.'
        << std::setw(6) << std::setfill('0')
        << (microseconds_since_epoch_ % 1000000);
    return oss.str();
}

std::string Timestamp::ToFormattedString(bool show_microseconds) const {
    const std::time_t seconds = static_cast<std::time_t>(SecondsSinceEpoch());
    std::tm tm_time {};
#if defined(_WIN32)
    localtime_s(&tm_time, &seconds);
#else
    // Linux 下使用线程安全版本，避免日志并发格式化时共享静态缓冲区。
    localtime_r(&seconds, &tm_time);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_time, "%F %T");
    if (show_microseconds) {
        oss << '.'
            << std::setw(6) << std::setfill('0')
            << (microseconds_since_epoch_ % 1000000);
    }
    return oss.str();
}

bool operator<(const Timestamp& lhs, const Timestamp& rhs) {
    return lhs.microseconds_since_epoch_ < rhs.microseconds_since_epoch_;
}

bool operator==(const Timestamp& lhs, const Timestamp& rhs) {
    return lhs.microseconds_since_epoch_ == rhs.microseconds_since_epoch_;
}

double TimeDifference(const Timestamp& high, const Timestamp& low) {
    const auto delta = static_cast<double>(high.MicrosecondsSinceEpoch())
        - static_cast<double>(low.MicrosecondsSinceEpoch());
    return delta / 1000000.0;
}

Timestamp AddTime(const Timestamp& timestamp, double seconds) {
    // 日志和定时器常用秒作为外部接口，内部仍转换成微秒存储。
    const auto delta = static_cast<std::uint64_t>(seconds * 1000000.0);
    return Timestamp(timestamp.MicrosecondsSinceEpoch() + delta);
}

}  // namespace runtime::time
