#pragma once

#include "runtime/base/noncopyable.h"

#include <cstddef>
#include <string>
#include <vector>

namespace runtime::net {

class Buffer : public runtime::base::NonCopyable {
public:
    static constexpr std::size_t kCheapPrepend = 8;
    static constexpr std::size_t kInitalSize = 1024;

    explicit Buffer(std::size_t inital_size = kInitalSize);

    std::size_t ReadableBytes() const;
    std::size_t WritableBytes() const;
    std::size_t PrependableBytes() const;

    const char *peek() const;

    void Retrieve(std::size_t len);
    void RetrieveUntil(const char *end);
    void RetrieveAll();

    std::string RetrieveAsString(std::size_t len);
    std::string RetrieveAllAsString();

    void append(const char *data, std::size_t len);
    void append(const std::string &str);

    char *BeginWrite();
    const char *BeginWrite() const;
    void HasWritten(std::size_t len);

    void EnsureWritableBytes(std::size_t len);

    ssize_t ReadFd(int fd, int *saved_errno);
    ssize_t WriteFd(int fd, int *saved_errno);
private:
    char *Begin();
    const char *Begin() const;
    void MakeSpace(std::size_t len);
private:
    std::vector<char> buffer_;
    std::size_t reader_index_;
    std::size_t writer_index_;
};
}
