#include "logforge/analyzer.hpp"

#include "logforge/parser.hpp"

#include <istream>
#include <ostream>
#include <string>

namespace logforge {

bool matches(const LogRecord& record, const Filters& filters)
{
    return (!filters.level || record.level == *filters.level)
           && (!filters.service || record.service == *filters.service);
}

AnalysisSummary analyze(
    std::istream& input,
    const Filters& filters,
    std::ostream* const record_output,
    std::ostream& diagnostics)
{
    AnalysisSummary summary;
    std::string line;

    while (std::getline(input, line)) {
        ++summary.lines_examined;
        const std::optional<LogRecord> record{parse_log_line(line)};

        if (!record) {
            ++summary.malformed_records;
            diagnostics << "Warning: malformed record at line " << summary.lines_examined << ": "
                        << line << '\n';
            continue;
        }

        ++summary.valid_records;
        if (!matches(*record, filters)) {
            continue;
        }

        ++summary.matched_records;
        ++summary.records_by_level[record->level];
        ++summary.records_by_service[record->service];

        if (record_output != nullptr) {
            print_record(*record, *record_output);
        }
    }

    return summary;
}

void print_record(const LogRecord& record, std::ostream& output)
{
    output << "timestamp=" << record.timestamp << " level=" << record.level
           << " service=" << record.service << " message=\"" << record.message << "\"\n";
}

void print_summary(const AnalysisSummary& summary, std::ostream& output)
{
    output << "Summary\n"
              "-------\n"
           << "Lines examined:    " << summary.lines_examined << '\n'
           << "Valid records:     " << summary.valid_records << '\n'
           << "Malformed records: " << summary.malformed_records << '\n'
           << "Matched records:   " << summary.matched_records << "\n\n"
              "By level (matched):\n";

    if (summary.records_by_level.empty()) {
        output << "(none)\n";
    } else {
        for (const auto& [level, count] : summary.records_by_level) {
            output << level << "  " << count << '\n';
        }
    }

    output << "\nBy service (matched):\n";
    if (summary.records_by_service.empty()) {
        output << "(none)\n";
    } else {
        for (const auto& [service, count] : summary.records_by_service) {
            output << service << "  " << count << '\n';
        }
    }
}

}  // namespace logforge
