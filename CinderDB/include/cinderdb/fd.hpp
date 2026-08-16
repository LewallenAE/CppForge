#pragma once

#include <unistd.h>
#include <utility>

namespace cinderdb {
class UniqueFd {
public:
    UniqueFd() = default;
    explicit UniqueFd(const int fd) noexcept : fd_{fd} {}
    ~UniqueFd() { reset(); }
    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;
    UniqueFd(UniqueFd&& other) noexcept : fd_{std::exchange(other.fd_, -1)} {}
    UniqueFd& operator=(UniqueFd&& other) noexcept { if (this != &other) reset(std::exchange(other.fd_, -1)); return *this; }
    [[nodiscard]] int get() const noexcept { return fd_; }
    [[nodiscard]] explicit operator bool() const noexcept { return fd_ >= 0; }
    [[nodiscard]] int release() noexcept { return std::exchange(fd_, -1); }
    void reset(const int replacement = -1) noexcept { if (fd_ >= 0) { ::close(fd_); } fd_ = replacement; }
private: int fd_{-1};
};
}  // namespace cinderdb
