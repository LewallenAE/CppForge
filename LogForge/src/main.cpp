#include "logforge/analyzer.hpp"
#include "logforge/options.hpp"

#include <fstream>
#include <iostream>
#include <string_view>

int main(const int argc, const char* const argv[])
{
    const logforge::OptionsResult parsed{logforge::parse_arguments(argc, argv)};
    const std::string_view program_name{argc > 0 ? argv[0] : "logforge"};

    if (!parsed.options) {
        std::cerr << "Error: " << parsed.error << '\n' << logforge::usage(program_name);
        return 2;
    }

    const logforge::Options& options{*parsed.options};
    if (options.help) {
        std::cout << logforge::usage(program_name);
        return 0;
    }

    std::ifstream input{options.filename};
    if (!input) {
        std::cerr << "Error: could not open file: " << options.filename << '\n';
        return 1;
    }

    const logforge::Filters filters{options.level, options.service};
    std::ostream* const record_output{options.summary ? nullptr : &std::cout};
    const logforge::AnalysisSummary summary{
        logforge::analyze(input, filters, record_output, std::cerr)};

    if (input.bad()) {
        std::cerr << "Error: failed while reading file: " << options.filename << '\n';
        return 1;
    }

    if (options.summary) {
        logforge::print_summary(summary, std::cout);
    }

    return 0;
}
