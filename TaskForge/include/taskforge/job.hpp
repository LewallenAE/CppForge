#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace taskforge {

using TaskId = std::uint64_t;

struct Job {
    TaskId id{0};
    std::chrono::milliseconds duration{0};
    bool should_fail{false};

    bool operator==(const Job&) const = default;
};

struct JobParseResult {
    std::optional<Job> job;
    std::string error;
};

[[nodiscard]] JobParseResult parse_job(std::string_view line);

}  // namespace taskforge
