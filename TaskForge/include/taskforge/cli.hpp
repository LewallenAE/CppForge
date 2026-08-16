#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace taskforge {

enum class Command {
    run,
    benchmark,
    help,
};

struct CliOptions {
    Command command{Command::help};
    std::string jobs_file;
    std::size_t worker_count{1};
    std::size_t queue_capacity{64};
    std::size_t task_count{100000};
};

struct CliResult {
    std::optional<CliOptions> options;
    std::string error;
};

[[nodiscard]] CliResult parse_cli(int argc, const char* const argv[]);
[[nodiscard]] std::string cli_usage(std::string_view program_name);

}  // namespace taskforge
