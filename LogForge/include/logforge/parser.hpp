#pragma once

#include "logforge/log_record.hpp"

#include <optional>
#include <string_view>

namespace logforge {

[[nodiscard]] std::optional<LogRecord> parse_log_line(std::string_view line);

}  // namespace logforge
