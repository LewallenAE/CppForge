#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace logforge {

struct Options {
    std::string filename;
    std::optional<std::string> level;
    std::optional<std::string> service;
    bool summary{false};
    bool help{false};
};

struct OptionsResult {
    std::optional<Options> options;
    std::string error;
};

[[nodiscard]] OptionsResult parse_arguments(int argc, const char* const argv[]);
[[nodiscard]] std::string usage(std::string_view program_name);

}  // namespace logforge
