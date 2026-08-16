#pragma once
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>
namespace cinderdb {
template <typename T> class BoundedQueue {
public:
 explicit BoundedQueue(std::size_t cap) : capacity_{cap} { if (cap == 0) throw std::invalid_argument{"queue capacity must be positive"}; }
 [[nodiscard]] bool push(T value) { std::unique_lock lock{mutex_}; not_full_.wait(lock,[this]{return closed_ || queue_.size()<capacity_;}); if(closed_) return false; queue_.push_back(std::move(value)); lock.unlock(); not_empty_.notify_one(); return true; }
 [[nodiscard]] std::optional<T> pop() { std::unique_lock lock{mutex_}; not_empty_.wait(lock,[this]{return closed_ || !queue_.empty();}); if(queue_.empty()) return std::nullopt; T value{std::move(queue_.front())}; queue_.pop_front(); lock.unlock(); not_full_.notify_one(); return value; }
 void close() { { std::lock_guard lock{mutex_}; closed_=true; } not_empty_.notify_all(); not_full_.notify_all(); }
private: std::size_t capacity_; std::mutex mutex_; std::condition_variable not_empty_,not_full_; std::deque<T> queue_; bool closed_{false};
};
}  // namespace cinderdb
