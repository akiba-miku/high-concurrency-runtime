#include "runtime/time/timestamp.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace runtime::time {

// Timestamp 内部统一用微秒保存时间，兼顾日志展示和后续定时器精度。
Timestamp Timestamp::now() {
    const auto now = std::chrono::system_clock::now();
    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch());
    return Timestamp(static_cast<runtime::base::u64>(micros.count()));
}

Timestamp Timestamp::invalid() {
    return Timestamp();
}

bool Timestamp::valid() const {
    return microseconds_since_epoch_ > 0;
}

runtime::base::u64 Timestamp::microsecondsSinceEpoch() const {
    return microseconds_since_epoch_;
}

runtime::base::u64 Timestamp::millisecondsSinceEpoch() const {
    return microseconds_since_epoch_ / 1000;
}

runtime::base::u64 Timestamp::secondsSinceEpoch() const {
    return microseconds_since_epoch_ / 1000000;
}

runtime::base::String Timestamp::toString() const {
    std::ostringstream oss;
    oss << secondsSinceEpoch() << '.'
        << std::setw(6) << std::setfill('0')
        << (microseconds_since_epoch_ % 1000000);
    return oss.str();
}

runtime::base::String Timestamp::toFormattedString(bool show_microseconds) const {
    const std::time_t seconds = static_cast<std::time_t>(secondsSinceEpoch());
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

double timeDifference(const Timestamp& high, const Timestamp& low) {
    const auto delta = static_cast<double>(high.microsecondsSinceEpoch())
        - static_cast<double>(low.microsecondsSinceEpoch());
    return delta / 1000000.0;
}

Timestamp addTime(const Timestamp& timestamp, double seconds) {
    // 日志和定时器常用秒作为外部接口，内部仍转换成微秒存储。
    const auto delta = static_cast<runtime::base::u64>(seconds * 1000000.0);
    return Timestamp(timestamp.microsecondsSinceEpoch() + delta);
}

}  // namespace runtime::time
