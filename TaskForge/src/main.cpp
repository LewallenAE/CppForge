#include "taskforge/cli.hpp"
#include "taskforge/job.hpp"
#include "taskforge/task_executor.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

struct SubmittedJob {
    taskforge::Job job;
    std::future<taskforge::TaskId> result;
};

void print_metrics(const taskforge::ExecutorMetrics& metrics)
{
    const auto elapsed_ms{
        std::chrono::duration<double, std::milli>{metrics.elapsed}.count()};
    std::cout << "workers=" << metrics.workers << " queue_capacity=" << metrics.queue_capacity
              << " peak_queue_depth=" << metrics.peak_queue_depth
              << " current_queue_depth=" << metrics.current_queue_depth
              << " submitted=" << metrics.tasks_submitted
              << " completed=" << metrics.tasks_completed << " failed=" << metrics.tasks_failed
              << " elapsed_ms=" << std::fixed << std::setprecision(2) << elapsed_ms << '\n';
}

int run_jobs(const taskforge::CliOptions& options)
{
    std::ifstream input{options.jobs_file};
    if (!input) {
        std::cerr << "Error: could not open jobs file: " << options.jobs_file << '\n';
        return 1;
    }

    std::vector<taskforge::Job> jobs;
    std::unordered_set<taskforge::TaskId> ids;
    std::string line;
    std::size_t line_number{0};

    while (std::getline(input, line)) {
        ++line_number;
        const taskforge::JobParseResult parsed{taskforge::parse_job(line)};
        if (!parsed.job) {
            std::cerr << "Error: malformed job at line " << line_number << ": " << parsed.error
                      << '\n';
            return 2;
        }

        const taskforge::Job job{*parsed.job};
        if (!ids.insert(job.id).second) {
            std::cerr << "Error: duplicate task id at line " << line_number << ": " << job.id
                      << '\n';
            return 2;
        }
        jobs.push_back(job);
    }

    if (input.bad()) {
        std::cerr << "Error: failed while reading jobs file: " << options.jobs_file << '\n';
        return 1;
    }

    taskforge::TaskExecutor executor{options.worker_count, options.queue_capacity};
    std::vector<SubmittedJob> submitted;
    submitted.reserve(jobs.size());
    for (const taskforge::Job& job : jobs) {
        std::future<taskforge::TaskId> result{executor.submit([job] {
            std::this_thread::sleep_for(job.duration);
            if (job.should_fail) {
                throw std::runtime_error{"task requested failure"};
            }
            return job.id;
        })};
        submitted.push_back(SubmittedJob{job, std::move(result)});
    }

    executor.shutdown();

    std::size_t observed_failures{0};
    for (SubmittedJob& item : submitted) {
        try {
            const taskforge::TaskId result_id{item.result.get()};
            std::cout << "task=" << result_id << " status=completed\n";
        } catch (const std::exception& error) {
            ++observed_failures;
            std::cout << "task=" << item.job.id << " status=failed error=\"" << error.what()
                      << "\"\n";
        }
    }

    print_metrics(executor.metrics());
    return observed_failures == 0 ? 0 : 3;
}

std::uint64_t benchmark_work(const std::uint64_t task_id)
{
    std::uint64_t value{task_id ^ 0x9E3779B97F4A7C15ULL};
    for (std::size_t iteration{0}; iteration < 2048; ++iteration) {
        value ^= value << 13U;
        value ^= value >> 7U;
        value ^= value << 17U;
        value += static_cast<std::uint64_t>(iteration) + 0xA0761D6478BD642FULL;
    }
    return value;
}

int run_benchmark(const taskforge::CliOptions& options)
{
    const auto started{std::chrono::steady_clock::now()};
    taskforge::TaskExecutor executor{options.worker_count, options.queue_capacity};
    std::vector<std::future<std::uint64_t>> futures;
    futures.reserve(options.task_count);

    for (std::size_t index{0}; index < options.task_count; ++index) {
        futures.push_back(executor.submit(
            [task_id = static_cast<std::uint64_t>(index)] { return benchmark_work(task_id); }));
    }

    executor.shutdown();

    std::uint64_t checksum{0};
    for (std::future<std::uint64_t>& future : futures) {
        checksum ^= future.get();
    }

    const auto elapsed{std::chrono::steady_clock::now() - started};
    const double seconds{std::chrono::duration<double>{elapsed}.count()};
    const double throughput{static_cast<double>(options.task_count) / seconds};
    const auto elapsed_ms{std::chrono::duration<double, std::milli>{elapsed}.count()};

    std::cout << "Benchmark\n"
              << "workers=" << options.worker_count << '\n'
              << "queue_capacity=" << options.queue_capacity << '\n'
              << "tasks=" << options.task_count << '\n'
              << "work_iterations_per_task=2048\n"
              << "elapsed_ms=" << std::fixed << std::setprecision(2) << elapsed_ms << '\n'
              << "tasks_per_second=" << std::fixed << std::setprecision(2) << throughput << '\n'
              << "checksum=" << checksum << '\n';
    print_metrics(executor.metrics());
    return 0;
}

}  // namespace

int main(const int argc, const char* const argv[])
{
    const taskforge::CliResult parsed{taskforge::parse_cli(argc, argv)};
    const std::string_view program_name{argc > 0 ? argv[0] : "taskforge"};

    if (!parsed.options) {
        std::cerr << "Error: " << parsed.error << '\n' << taskforge::cli_usage(program_name);
        return 2;
    }

    if (parsed.options->command == taskforge::Command::help) {
        std::cout << taskforge::cli_usage(program_name);
        return 0;
    }

    try {
        if (parsed.options->command == taskforge::Command::run) {
            return run_jobs(*parsed.options);
        }
        return run_benchmark(*parsed.options);
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
