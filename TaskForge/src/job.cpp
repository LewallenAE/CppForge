#include "taskforge/job.hpp"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>

namespace taskforge {
namespace {

void skip_whitespace(const std::string_view line, std::size_t& position)
{
    while (position < line.size()
           && (line[position] == ' ' || line[position] == '\t' || line[position] == '\r')) {
        ++position;
    }
}

std::string_view next_token(const std::string_view line, std::size_t& position)
{
    skip_whitespace(line, position);
    const std::size_t beginning{position};
    while (position < line.size() && line[position] != ' ' && line[position] != '\t'
           && line[position] != '\r') {
        ++position;
    }
    return line.substr(beginning, position - beginning);
}

bool parse_unsigned(const std::string_view token, std::uint64_t& value)
{
    if (token.empty()) {
        return false;
    }

    const char* const beginning{token.data()};
    const char* const ending{token.data() + token.size()};
    const auto [position, error]{std::from_chars(beginning, ending, value)};
    return error == std::errc{} && position == ending;
}

JobParseResult failure(std::string error)
{
    return JobParseResult{std::nullopt, std::move(error)};
}

}  // namespace

JobParseResult parse_job(const std::string_view line)
{
    std::size_t position{0};
    const std::string_view id_token{next_token(line, position)};
    const std::string_view duration_token{next_token(line, position)};
    const std::string_view outcome_token{next_token(line, position)};
    const std::string_view extra_token{next_token(line, position)};

    std::uint64_t id{0};
    if (!parse_unsigned(id_token, id)) {
        return failure("task id must be an unsigned integer");
    }

    std::uint64_t duration{0};
    if (!parse_unsigned(duration_token, duration)) {
        return failure("duration must be an unsigned integer number of milliseconds");
    }
    if (duration > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return failure("duration is too large");
    }

    if (!extra_token.empty()) {
        return failure("expected: <task-id> <duration-ms> [fail]");
    }
    if (!outcome_token.empty() && outcome_token != "fail") {
        return failure("optional third field must be 'fail'");
    }

    return JobParseResult{
        Job{id, std::chrono::milliseconds{static_cast<std::int64_t>(duration)},
            outcome_token == "fail"},
        {},
    };
}

}  // namespace taskforge
