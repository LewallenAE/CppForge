#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>

namespace taskforge {

template <typename T>
class BoundedQueue {
public:
    explicit BoundedQueue(const std::size_t capacity)
        : capacity_{capacity}
    {
        if (capacity == 0) {
            throw std::invalid_argument{"queue capacity must be greater than zero"};
        }
    }

    BoundedQueue(const BoundedQueue&) = delete;
    BoundedQueue& operator=(const BoundedQueue&) = delete;
    BoundedQueue(BoundedQueue&&) = delete;
    BoundedQueue& operator=(BoundedQueue&&) = delete;

    [[nodiscard]] bool push(T value)
    {
        std::unique_lock lock{mutex_};
        not_full_.wait(lock, [this] { return closed_ || queue_.size() < capacity_; });

        if (closed_) {
            return false;
        }

        queue_.push_back(std::move(value));
        if (queue_.size() > peak_size_) {
            peak_size_ = queue_.size();
        }

        lock.unlock();
        not_empty_.notify_one();
        return true;
    }

    [[nodiscard]] std::optional<T> pop()
    {
        std::unique_lock lock{mutex_};
        not_empty_.wait(lock, [this] { return closed_ || !queue_.empty(); });

        if (queue_.empty()) {
            return std::nullopt;
        }

        T value{std::move(queue_.front())};
        queue_.pop_front();
        lock.unlock();
        not_full_.notify_one();
        return value;
    }

    void close()
    {
        {
            std::lock_guard lock{mutex_};
            closed_ = true;
        }
        not_empty_.notify_all();
        not_full_.notify_all();
    }

    [[nodiscard]] bool is_closed() const
    {
        std::lock_guard lock{mutex_};
        return closed_;
    }

    [[nodiscard]] std::size_t size() const
    {
        std::lock_guard lock{mutex_};
        return queue_.size();
    }

    [[nodiscard]] std::size_t peak_size() const
    {
        std::lock_guard lock{mutex_};
        return peak_size_;
    }

    [[nodiscard]] std::size_t capacity() const noexcept
    {
        return capacity_;
    }

private:
    const std::size_t capacity_;
    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    std::deque<T> queue_;
    std::size_t peak_size_{0};
    bool closed_{false};
};

}  // namespace taskforge
