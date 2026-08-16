#pragma once
#include <optional>
#include <string>
#include <string_view>
namespace cinderdb {
constexpr std::size_t max_request_line_size{4096};
enum class CommandType { put, get, erase };
struct Command { CommandType type; std::string key; std::string value; };
struct ParseResult { std::optional<Command> command; std::string error; };
[[nodiscard]] ParseResult parse_command(std::string_view line);
[[nodiscard]] bool valid_key(std::string_view key);
}  // namespace cinderdb
