#pragma once

#include "logforge/log_record.hpp"

#include <cstddef>
#include <iosfwd>
#include <map>
#include <optional>
#include <string>

namespace logforge {

struct Filters {
    std::optional<std::string> level;
    std::optional<std::string> service;
};

struct AnalysisSummary {
    std::size_t lines_examined{0};
    std::size_t valid_records{0};
    std::size_t malformed_records{0};
    std::size_t matched_records{0};
    std::map<std::string, std::size_t> records_by_level;
    std::map<std::string, std::size_t> records_by_service;

    bool operator==(const AnalysisSummary&) const = default;
};

[[nodiscard]] bool matches(const LogRecord& record, const Filters& filters);

[[nodiscard]] AnalysisSummary analyze(
    std::istream& input,
    const Filters& filters,
    std::ostream* record_output,
    std::ostream& diagnostics);

void print_record(const LogRecord& record, std::ostream& output);
void print_summary(const AnalysisSummary& summary, std::ostream& output);

}  // namespace logforge
