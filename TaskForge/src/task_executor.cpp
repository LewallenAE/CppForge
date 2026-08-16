#include "taskforge/task_executor.hpp"

#include <chrono>
#include <exception>
#include <stdexcept>

namespace taskforge {

TaskExecutor::TaskExecutor(const std::size_t worker_count, const std::size_t queue_capacity)
    : queue_{queue_capacity}
    , started_at_{std::chrono::steady_clock::now()}
{
    if (worker_count == 0) {
        throw std::invalid_argument{"worker count must be greater than zero"};
    }

    workers_.reserve(worker_count);
    try {
        for (std::size_t index{0}; index < worker_count; ++index) {
            workers_.emplace_back([this] { worker_loop(); });
        }
    } catch (...) {
        accepting_ = false;
        queue_.close();
        for (std::thread& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        throw;
    }
}

TaskExecutor::~TaskExecutor()
{
    shutdown();
}

void TaskExecutor::shutdown() noexcept
{
    try {
        std::call_once(shutdown_once_, [this] {
            {
                std::lock_guard lock{submission_mutex_};
                accepting_ = false;
            }
            queue_.close();

            {
                std::unique_lock lock{submission_mutex_};
                no_active_submissions_.wait(lock, [this] { return active_submissions_ == 0; });
            }

            for (std::thread& worker : workers_) {
                if (worker.joinable()) {
                    worker.join();
                }
            }

            const auto elapsed{std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - started_at_)};
            stopped_after_nanoseconds_.store(elapsed.count());
        });
    } catch (...) {
        std::terminate();
    }
}

void TaskExecutor::begin_submission()
{
    std::lock_guard lock{submission_mutex_};
    if (!accepting_) {
        throw SubmissionRejected{};
    }
    ++active_submissions_;
}

void TaskExecutor::end_submission() noexcept
{
    {
        std::lock_guard lock{submission_mutex_};
        --active_submissions_;
    }
    no_active_submissions_.notify_all();
}

ExecutorMetrics TaskExecutor::metrics() const noexcept
{
    const std::int64_t stopped{stopped_after_nanoseconds_.load()};
    const auto elapsed = stopped >= 0
                             ? std::chrono::nanoseconds{stopped}
                             : std::chrono::duration_cast<std::chrono::nanoseconds>(
                                   std::chrono::steady_clock::now() - started_at_);

    return ExecutorMetrics{
        workers_.size(),
        queue_.capacity(),
        queue_.size(),
        queue_.peak_size(),
        tasks_submitted_.load(),
        tasks_completed_.load(),
        tasks_failed_.load(),
        elapsed,
    };
}

std::size_t TaskExecutor::worker_count() const noexcept
{
    return workers_.size();
}

void TaskExecutor::worker_loop() noexcept
{
    while (std::optional<std::packaged_task<void()>> task = queue_.pop()) {
        try {
            (*task)();
        } catch (...) {
            tasks_failed_.fetch_add(1);
        }
    }
}

}  // namespace taskforge
