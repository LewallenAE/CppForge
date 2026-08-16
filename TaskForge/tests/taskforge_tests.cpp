#include "taskforge/bounded_queue.hpp"
#include "taskforge/cli.hpp"
#include "taskforge/job.hpp"
#include "taskforge/task_executor.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <future>
#include <initializer_list>
#include <iostream>
#include <latch>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

int failures{0};

void check(const bool condition, const std::string_view description)
{
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        ++failures;
    }
}

template <typename Exception, typename Function>
void check_throws(Function&& function, const std::string_view description)
{
    try {
        std::forward<Function>(function)();
        check(false, description);
    } catch (const Exception&) {
        check(true, description);
    } catch (...) {
        check(false, description);
    }
}

taskforge::CliResult parse_cli(std::initializer_list<const char*> arguments)
{
    return taskforge::parse_cli(static_cast<int>(arguments.size()), arguments.begin());
}

void test_job_parser()
{
    const auto ordinary{taskforge::parse_job("42 25")};
    check(ordinary.job == taskforge::Job{42, std::chrono::milliseconds{25}, false},
          "job parser accepts id and duration");

    const auto failure{taskforge::parse_job("7\t100 fail")};
    check(failure.job == taskforge::Job{7, std::chrono::milliseconds{100}, true},
          "job parser accepts explicit failure jobs");
    check(taskforge::parse_job("8 5\r").job.has_value(),
          "job parser accepts CRLF input after line extraction");

    check(!taskforge::parse_job("").job, "job parser rejects empty lines");
    check(!taskforge::parse_job("abc 10").job, "job parser rejects invalid ids");
    check(!taskforge::parse_job("1 -2").job, "job parser rejects negative durations");
    check(!taskforge::parse_job("1 2 maybe").job, "job parser rejects invalid outcomes");
    check(!taskforge::parse_job("1 2 fail extra").job, "job parser rejects extra fields");
}

void test_cli_parser()
{
    const auto run{parse_cli(
        {"taskforge", "run", "jobs.txt", "--workers", "4", "--queue-capacity", "8"})};
    check(run.options && run.options->command == taskforge::Command::run
              && run.options->jobs_file == "jobs.txt" && run.options->worker_count == 4
              && run.options->queue_capacity == 8,
          "CLI parses run configuration");

    const auto flexible{parse_cli(
        {"taskforge", "run", "--queue-capacity", "3", "jobs.txt", "--workers", "2"})};
    check(flexible.options && flexible.options->worker_count == 2
              && flexible.options->queue_capacity == 3,
          "CLI accepts sensible option ordering");

    const auto benchmark{parse_cli(
        {"taskforge", "benchmark", "--workers", "8", "--tasks", "1000",
         "--queue-capacity", "16"})};
    check(benchmark.options && benchmark.options->command == taskforge::Command::benchmark
              && benchmark.options->task_count == 1000 && benchmark.options->worker_count == 8,
          "CLI parses benchmark configuration");

    check(!parse_cli({"taskforge"}).options, "CLI rejects a missing command");
    check(!parse_cli({"taskforge", "unknown"}).options, "CLI rejects an unknown command");
    check(!parse_cli({"taskforge", "run", "jobs.txt", "--wat"}).options,
          "CLI rejects an unknown option");
    check(!parse_cli({"taskforge", "run", "jobs.txt", "--workers", "0"}).options,
          "CLI rejects zero workers");
    check(!parse_cli({"taskforge", "run", "jobs.txt", "--workers", "abc"}).options,
          "CLI rejects an invalid worker count");
    check(!parse_cli({"taskforge", "run", "jobs.txt", "--queue-capacity", "0"}).options,
          "CLI rejects zero queue capacity");
    check(!parse_cli({"taskforge", "run", "--workers"}).options,
          "CLI rejects missing option values");
    check(!parse_cli({"taskforge", "run"}).options, "CLI rejects a missing jobs file");
    check(!parse_cli({"taskforge", "run", "jobs.txt", "--tasks", "10"}).options,
          "CLI rejects benchmark-only options in run mode");
}

void test_bounded_queue_basic()
{
    taskforge::BoundedQueue<int> queue{3};
    check(queue.capacity() == 3 && queue.size() == 0, "queue reports its capacity and size");
    check(queue.push(10) && queue.push(20) && queue.push(30), "queue accepts values");
    check(queue.size() == 3 && queue.peak_size() == 3, "queue enforces and records capacity");
    check(queue.pop() == 10 && queue.pop() == 20 && queue.pop() == 30,
          "queue preserves FIFO order");

    queue.close();
    check(queue.is_closed(), "queue reports closed state");
    check(!queue.push(40), "closed queue rejects pushes");
    check(!queue.pop(), "closed empty queue returns no value");
    queue.close();

    check_throws<std::invalid_argument>([] { taskforge::BoundedQueue<int> invalid{0}; },
                                        "queue rejects zero capacity");
}

void test_queue_blocking_and_wakeup()
{
    using namespace std::chrono_literals;

    {
        taskforge::BoundedQueue<int> queue{1};
        check(queue.push(1), "queue setup push succeeds");
        std::latch started{1};
        std::promise<bool> pushed;
        std::future<bool> result{pushed.get_future()};
        std::thread producer{[&] {
            started.count_down();
            pushed.set_value(queue.push(2));
        }};

        started.wait();
        check(result.wait_for(0ms) == std::future_status::timeout && queue.size() == 1,
              "producer cannot complete while queue is full");
        check(queue.pop() == 1, "pop creates capacity for blocked producer");
        producer.join();
        check(result.get() && queue.pop() == 2, "blocked producer wakes when space is available");
        queue.close();
    }

    {
        taskforge::BoundedQueue<int> queue{1};
        std::latch started{1};
        std::promise<std::optional<int>> popped;
        std::future<std::optional<int>> result{popped.get_future()};
        std::thread consumer{[&] {
            started.count_down();
            popped.set_value(queue.pop());
        }};

        started.wait();
        check(result.wait_for(0ms) == std::future_status::timeout,
              "consumer cannot complete while queue is empty");
        check(queue.push(7), "push supplies work to blocked consumer");
        consumer.join();
        check(result.get() == 7, "blocked consumer wakes when work arrives");
        queue.close();
    }
}

void test_queue_close_wakes_waiters()
{
    {
        taskforge::BoundedQueue<int> queue{1};
        check(queue.push(1), "close-producer setup succeeds");
        std::latch started{1};
        std::promise<bool> pushed;
        std::future<bool> result{pushed.get_future()};
        std::thread producer{[&] {
            started.count_down();
            pushed.set_value(queue.push(2));
        }};
        started.wait();
        queue.close();
        producer.join();
        check(!result.get(), "close wakes and rejects a blocked producer");
        check(queue.pop() == 1 && !queue.pop(), "close drains accepted work then ends pops");
    }

    {
        taskforge::BoundedQueue<int> queue{1};
        std::latch started{1};
        std::promise<std::optional<int>> popped;
        std::future<std::optional<int>> result{popped.get_future()};
        std::thread consumer{[&] {
            started.count_down();
            popped.set_value(queue.pop());
        }};
        started.wait();
        queue.close();
        consumer.join();
        check(!result.get(), "close wakes a blocked consumer without fabricating work");
    }
}

void test_executor_results_and_failures()
{
    taskforge::TaskExecutor executor{3, 2};
    check(executor.worker_count() == 3, "executor honors configured worker count");

    std::future<int> value{executor.submit([] { return 42; })};
    auto owned_value = std::make_unique<int>(19);
    std::future<int> move_only{
        executor.submit([value = std::move(owned_value)] { return *value; })};
    std::future<int> failure{executor.submit([]() -> int {
        throw std::runtime_error{"expected task failure"};
    })};
    std::future<int> after_failure{executor.submit([] { return 7; })};

    executor.shutdown();
    check(value.get() == 42, "executor returns task results");
    check(move_only.get() == 19, "executor accepts move-only callables");
    check_throws<std::runtime_error>([&] { static_cast<void>(failure.get()); },
                                     "task exception reaches the future");
    check(after_failure.get() == 7, "worker remains usable after task exception");

    const taskforge::ExecutorMetrics metrics{executor.metrics()};
    check(metrics.tasks_submitted == 4 && metrics.tasks_completed == 3
              && metrics.tasks_failed == 1,
          "executor metrics distinguish completed and failed tasks");
    check(metrics.current_queue_depth == 0 && metrics.peak_queue_depth <= 2,
          "executor reports bounded queue depth");

    executor.shutdown();
    check_throws<taskforge::SubmissionRejected>(
        [&] { static_cast<void>(executor.submit([] {})); },
        "executor rejects submission after shutdown");
}

void test_executor_concurrency_and_lifetime()
{
    constexpr std::ptrdiff_t concurrent_tasks{4};
    taskforge::TaskExecutor executor{4, 4};
    std::latch all_running{concurrent_tasks};
    std::latch release{1};
    std::vector<std::future<void>> futures;

    for (std::ptrdiff_t index{0}; index < concurrent_tasks; ++index) {
        futures.push_back(executor.submit([&] {
            all_running.count_down();
            release.wait();
        }));
    }

    all_running.wait();
    check(true, "four workers execute tasks concurrently");
    release.count_down();
    executor.shutdown();
    for (std::future<void>& future : futures) {
        future.get();
    }

    std::future<int> destructor_result;
    {
        taskforge::TaskExecutor scoped_executor{2, 1};
        destructor_result = scoped_executor.submit([] { return 88; });
    }
    check(destructor_result.get() == 88, "executor destructor drains accepted tasks safely");

    check_throws<std::invalid_argument>([] { taskforge::TaskExecutor invalid{0, 1}; },
                                        "executor rejects zero workers");
}

void test_shutdown_wakes_blocked_submission()
{
    taskforge::TaskExecutor executor{1, 1};
    std::latch worker_started{1};
    std::latch release_worker{1};

    std::future<void> running{executor.submit([&] {
        worker_started.count_down();
        release_worker.wait();
    })};
    worker_started.wait();
    std::future<int> queued{executor.submit([] { return 2; })};

    std::latch submitter_started{1};
    std::promise<bool> rejected_promise;
    std::future<bool> rejected{rejected_promise.get_future()};
    std::thread blocked_submitter{[&] {
        submitter_started.count_down();
        try {
            static_cast<void>(executor.submit([] { return 3; }));
            rejected_promise.set_value(false);
        } catch (const taskforge::SubmissionRejected&) {
            rejected_promise.set_value(true);
        }
    }};
    submitter_started.wait();

    std::thread shutdown_thread{[&] { executor.shutdown(); }};
    check(rejected.get(), "shutdown wakes and rejects a producer blocked on a full task queue");
    release_worker.count_down();
    blocked_submitter.join();
    shutdown_thread.join();

    running.get();
    check(queued.get() == 2, "shutdown drains tasks accepted before queue close");
    const auto metrics{executor.metrics()};
    check(metrics.tasks_submitted == 2 && metrics.tasks_completed == 2
              && metrics.tasks_failed == 0,
          "shutdown waits for in-flight submissions and leaves stable final metrics");
}

void test_executor_many_tasks()
{
    taskforge::TaskExecutor executor{4, 8};
    constexpr std::size_t task_count{2000};
    std::atomic<std::size_t> executed{0};
    std::vector<std::future<void>> futures;
    futures.reserve(task_count);

    for (std::size_t index{0}; index < task_count; ++index) {
        futures.push_back(executor.submit([&executed] { executed.fetch_add(1); }));
    }
    executor.shutdown();
    for (std::future<void>& future : futures) {
        future.get();
    }

    check(executed.load() == task_count, "executor completes many submitted tasks");
    const auto metrics{executor.metrics()};
    check(metrics.tasks_submitted == task_count && metrics.tasks_completed == task_count
              && metrics.tasks_failed == 0,
          "shutdown completes all accepted tasks before returning");
}

void test_executor_stress()
{
    constexpr std::size_t producer_count{4};
    constexpr std::size_t tasks_per_producer{3000};
    constexpr std::size_t total_tasks{producer_count * tasks_per_producer};
    constexpr std::size_t worker_count{8};
    constexpr std::size_t queue_capacity{7};

    taskforge::TaskExecutor executor{worker_count, queue_capacity};
    std::vector<std::atomic<unsigned int>> executions(total_tasks);
    std::vector<std::vector<std::future<std::size_t>>> futures(producer_count);
    std::vector<std::exception_ptr> producer_errors(producer_count);
    std::vector<std::thread> producers;
    producers.reserve(producer_count);

    for (std::size_t producer{0}; producer < producer_count; ++producer) {
        futures[producer].reserve(tasks_per_producer);
        producers.emplace_back([&, producer] {
            try {
                for (std::size_t offset{0}; offset < tasks_per_producer; ++offset) {
                    const std::size_t id{producer * tasks_per_producer + offset};
                    futures[producer].push_back(executor.submit([&, id] {
                        executions[id].fetch_add(1U);
                        if (id % 257U == 0U) {
                            throw std::runtime_error{"deterministic stress failure"};
                        }
                        return id;
                    }));
                }
            } catch (...) {
                producer_errors[producer] = std::current_exception();
            }
        });
    }

    for (std::thread& producer : producers) {
        producer.join();
    }
    for (const std::exception_ptr& error : producer_errors) {
        check(error == nullptr, "stress producer submitted every task");
    }

    executor.shutdown();

    std::size_t observed_completed{0};
    std::size_t observed_failed{0};
    for (auto& producer_futures : futures) {
        for (std::future<std::size_t>& future : producer_futures) {
            try {
                static_cast<void>(future.get());
                ++observed_completed;
            } catch (const std::runtime_error&) {
                ++observed_failed;
            }
        }
    }

    std::size_t expected_failed{0};
    bool exactly_once{true};
    for (std::size_t id{0}; id < total_tasks; ++id) {
        if (id % 257U == 0U) {
            ++expected_failed;
        }
        if (executions[id].load() != 1U) {
            exactly_once = false;
        }
    }

    const taskforge::ExecutorMetrics metrics{executor.metrics()};
    check(exactly_once, "stress executes every deterministic task id exactly once");
    check(observed_failed == expected_failed && observed_completed + observed_failed == total_tasks,
          "stress futures account for every success and failure");
    check(metrics.tasks_submitted == total_tasks
              && metrics.tasks_completed == observed_completed
              && metrics.tasks_failed == observed_failed,
          "stress invariant submitted equals completed plus failed");
    check(metrics.peak_queue_depth <= queue_capacity,
          "stress never exceeds the configured queue capacity");

    std::cout << "Stress: producers=" << producer_count << " workers=" << worker_count
              << " queue_capacity=" << queue_capacity << " tasks=" << total_tasks
              << " completed=" << observed_completed << " failed=" << observed_failed << '\n';
}

}  // namespace

int main()
{
    test_job_parser();
    test_cli_parser();
    test_bounded_queue_basic();
    test_queue_blocking_and_wakeup();
    test_queue_close_wakes_waiters();
    test_executor_results_and_failures();
    test_executor_concurrency_and_lifetime();
    test_shutdown_wakes_blocked_submission();
    test_executor_many_tasks();
    test_executor_stress();

    if (failures != 0) {
        std::cerr << failures << " test assertion(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All TaskForge tests passed\n";
    return EXIT_SUCCESS;
}
