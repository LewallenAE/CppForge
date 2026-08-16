#pragma once

#include "taskforge/bounded_queue.hpp"
#include "taskforge/metrics.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <condition_variable>
#include <exception>
#include <functional>
#include <future>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace taskforge {

class SubmissionRejected : public std::runtime_error {
public:
    SubmissionRejected()
        : std::runtime_error{"task submission rejected: executor is shutting down"}
    {
    }
};

class TaskExecutor {
public:
    TaskExecutor(std::size_t worker_count, std::size_t queue_capacity);
    ~TaskExecutor();

    TaskExecutor(const TaskExecutor&) = delete;
    TaskExecutor& operator=(const TaskExecutor&) = delete;
    TaskExecutor(TaskExecutor&&) = delete;
    TaskExecutor& operator=(TaskExecutor&&) = delete;

    template <typename Function>
        requires std::invocable<std::decay_t<Function>&>
    [[nodiscard]] auto submit(Function&& function)
        -> std::future<std::invoke_result_t<std::decay_t<Function>&>>
    {
        using Callable = std::decay_t<Function>;
        using Result = std::invoke_result_t<Callable&>;

        std::promise<Result> promise;
        std::future<Result> future{promise.get_future()};

        auto work = [this,
                     callable = Callable{std::forward<Function>(function)},
                     promise = std::move(promise)]() mutable {
            try {
                if constexpr (std::is_void_v<Result>) {
                    std::invoke(callable);
                    promise.set_value();
                } else {
                    promise.set_value(std::invoke(callable));
                }
                tasks_completed_.fetch_add(1);
            } catch (...) {
                tasks_failed_.fetch_add(1);
                try {
                    promise.set_exception(std::current_exception());
                } catch (...) {
                    // The promise is private to this task and should be unsatisfied here.
                }
            }
        };

        std::packaged_task<void()> task{std::move(work)};
        begin_submission();

        bool accepted{false};
        try {
            accepted = queue_.push(std::move(task));
            if (accepted) {
                tasks_submitted_.fetch_add(1);
            }
        } catch (...) {
            end_submission();
            throw;
        }
        end_submission();

        if (!accepted) {
            throw SubmissionRejected{};
        }
        return future;
    }

    void shutdown() noexcept;
    [[nodiscard]] ExecutorMetrics metrics() const noexcept;
    [[nodiscard]] std::size_t worker_count() const noexcept;

private:
    void begin_submission();
    void end_submission() noexcept;
    void worker_loop() noexcept;

    BoundedQueue<std::packaged_task<void()>> queue_;
    std::vector<std::thread> workers_;
    std::atomic<std::size_t> tasks_submitted_{0};
    std::atomic<std::size_t> tasks_completed_{0};
    std::atomic<std::size_t> tasks_failed_{0};
    std::mutex submission_mutex_;
    std::condition_variable no_active_submissions_;
    std::size_t active_submissions_{0};
    bool accepting_{true};
    std::once_flag shutdown_once_;
    const std::chrono::steady_clock::time_point started_at_;
    std::atomic<std::int64_t> stopped_after_nanoseconds_{-1};
};

}  // namespace taskforge
