#include "logforge/options.hpp"

#include <string_view>
#include <utility>

namespace logforge {
namespace {

bool looks_like_option(const std::string_view argument)
{
    return argument.starts_with('-');
}

OptionsResult failure(std::string message)
{
    return OptionsResult{std::nullopt, std::move(message)};
}

}  // namespace

OptionsResult parse_arguments(const int argc, const char* const argv[])
{
    Options options;

    for (int index{1}; index < argc; ++index) {
        const std::string_view argument{argv[index]};

        if (argument == "--help") {
            options.help = true;
            continue;
        }

        if (argument == "--summary") {
            options.summary = true;
            continue;
        }

        if (argument == "--level" || argument == "--service") {
            if (index + 1 >= argc || looks_like_option(argv[index + 1])) {
                return failure("missing value for " + std::string{argument});
            }

            const std::string value{argv[++index]};
            if (argument == "--level") {
                if (options.level) {
                    return failure("option specified more than once: --level");
                }
                options.level = value;
            } else {
                if (options.service) {
                    return failure("option specified more than once: --service");
                }
                options.service = value;
            }
            continue;
        }

        if (looks_like_option(argument)) {
            return failure("unknown option: " + std::string{argument});
        }

        if (!options.filename.empty()) {
            return failure("multiple input files specified: " + options.filename + " and "
                           + std::string{argument});
        }
        options.filename = argument;
    }

    if (options.filename.empty() && !options.help) {
        return failure("missing log filename");
    }

    return OptionsResult{std::move(options), {}};
}

std::string usage(const std::string_view program_name)
{
    return "Usage: " + std::string{program_name}
           + " <log-file> [--level <LEVEL>] [--service <SERVICE>] [--summary]\n"
             "       "
           + std::string{program_name} + " --help\n";
}

}  // namespace logforge
