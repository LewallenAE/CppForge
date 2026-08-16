#pragma once

#include <string>

namespace logforge {

struct LogRecord {
    std::string timestamp;
    std::string level;
    std::string service;
    std::string message;

    bool operator==(const LogRecord&) const = default;
};

}  // namespace logforge
