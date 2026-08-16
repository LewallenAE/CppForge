#include "taskforge/cli.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>

namespace taskforge {
namespace {

CliResult failure(std::string error)
{
    return CliResult{std::nullopt, std::move(error)};
}

std::size_t default_worker_count()
{
    return std::max<std::size_t>(1, static_cast<std::size_t>(std::thread::hardware_concurrency()));
}

bool parse_positive_size(const std::string_view text, std::size_t& value)
{
    std::uint64_t parsed{0};
    const char* const beginning{text.data()};
    const char* const ending{text.data() + text.size()};
    const auto [position, error]{std::from_chars(beginning, ending, parsed)};

    if (text.empty() || error != std::errc{} || position != ending || parsed == 0
        || parsed > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        return false;
    }

    value = static_cast<std::size_t>(parsed);
    return true;
}

bool looks_like_option(const std::string_view text)
{
    return text.starts_with('-');
}

}  // namespace

CliResult parse_cli(const int argc, const char* const argv[])
{
    if (argc < 2) {
        return failure("missing command");
    }

    CliOptions options;
    options.worker_count = default_worker_count();

    const std::string_view command{argv[1]};
    if (command == "--help" || command == "-h") {
        options.command = Command::help;
        return CliResult{std::move(options), {}};
    }
    if (command == "run") {
        options.command = Command::run;
    } else if (command == "benchmark") {
        options.command = Command::benchmark;
        options.queue_capacity = 256;
    } else {
        return failure("unknown command: " + std::string{command});
    }

    bool workers_seen{false};
    bool capacity_seen{false};
    bool tasks_seen{false};

    for (int index{2}; index < argc; ++index) {
        const std::string_view argument{argv[index]};

        if (argument == "--help" || argument == "-h") {
            options.command = Command::help;
            return CliResult{std::move(options), {}};
        }

        if (argument == "--workers" || argument == "--queue-capacity" || argument == "--tasks") {
            if (index + 1 >= argc || looks_like_option(argv[index + 1])) {
                return failure("missing value for " + std::string{argument});
            }

            std::size_t value{0};
            if (!parse_positive_size(argv[++index], value)) {
                return failure("invalid positive integer for " + std::string{argument} + ": "
                               + argv[index]);
            }

            if (argument == "--workers") {
                if (workers_seen) {
                    return failure("option specified more than once: --workers");
                }
                workers_seen = true;
                options.worker_count = value;
            } else if (argument == "--queue-capacity") {
                if (capacity_seen) {
                    return failure("option specified more than once: --queue-capacity");
                }
                capacity_seen = true;
                options.queue_capacity = value;
            } else {
                if (options.command != Command::benchmark) {
                    return failure("--tasks is only valid for the benchmark command");
                }
                if (tasks_seen) {
                    return failure("option specified more than once: --tasks");
                }
                tasks_seen = true;
                options.task_count = value;
            }
            continue;
        }

        if (looks_like_option(argument)) {
            return failure("unknown option: " + std::string{argument});
        }

        if (options.command != Command::run) {
            return failure("unexpected positional argument: " + std::string{argument});
        }
        if (!options.jobs_file.empty()) {
            return failure("multiple jobs files specified");
        }
        options.jobs_file = argument;
    }

    if (options.command == Command::run && options.jobs_file.empty()) {
        return failure("missing jobs filename for run command");
    }

    return CliResult{std::move(options), {}};
}

std::string cli_usage(const std::string_view program_name)
{
    const std::string program{program_name};
    return "Usage:\n  " + program
           + " run <jobs-file> [--workers <N>] [--queue-capacity <N>]\n  " + program
           + " benchmark [--workers <N>] [--queue-capacity <N>] [--tasks <N>]\n  "
           + program + " --help\n";
}

}  // namespace taskforge
