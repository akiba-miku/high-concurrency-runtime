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

    std::size_t readableBytes() const;
    std::size_t writableBytes() const;
    std::size_t prependableBytes() const;

    const char *peek() const;

    void retrieve(std::size_t len);
    void retrieveUntil(const char *end);
    void retrieveAll();

    std::string retrieveAsString(std::size_t len);
    std::string retrieveAllAsString();

    void append(const char *data, std::size_t len);
    void append(const std::string &str);

    char *beginWrite();
    const char *beginWrite() const;
    void hasWritten(std::size_t len);

    void ensureWritableBytes(std::size_t len);

    ssize_t readFd(int fd, int *saved_errno);
    ssize_t writeFd(int fd, int *saved_errno);
private:
    char *begin();
    const char *begin() const;
    void makeSpace(std::size_t len);
private:
    std::vector<char> buffer_;
    std::size_t reader_index_;
    std::size_t writer_index_;
};
}