#include "logforge/parser.hpp"

#include <cstddef>
#include <string_view>

namespace logforge {
namespace {

void skip_spaces(std::string_view input, std::size_t& position)
{
    while (position < input.size() && (input[position] == ' ' || input[position] == '\t')) {
        ++position;
    }
}

std::string_view next_field(std::string_view input, std::size_t& position)
{
    skip_spaces(input, position);
    const std::size_t beginning{position};
    while (position < input.size() && input[position] != ' ' && input[position] != '\t') {
        ++position;
    }
    return input.substr(beginning, position - beginning);
}

}  // namespace

std::optional<LogRecord> parse_log_line(const std::string_view line)
{
    std::size_t position{0};
    const std::string_view timestamp{next_field(line, position)};
    const std::string_view level{next_field(line, position)};
    const std::string_view service{next_field(line, position)};

    skip_spaces(line, position);
    const std::string_view message{line.substr(position)};

    if (timestamp.empty() || level.empty() || service.empty() || message.empty()) {
        return std::nullopt;
    }

    return LogRecord{
        std::string{timestamp},
        std::string{level},
        std::string{service},
        std::string{message},
    };
}

}  // namespace logforge
