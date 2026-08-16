#include "logforge/analyzer.hpp"
#include "logforge/options.hpp"
#include "logforge/parser.hpp"

#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

namespace {

int failures{0};

void check(const bool condition, const std::string_view description)
{
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        ++failures;
    }
}

logforge::OptionsResult parse(std::initializer_list<const char*> arguments)
{
    return logforge::parse_arguments(
        static_cast<int>(arguments.size()), arguments.begin());
}

void test_parser()
{
    const auto normal{logforge::parse_log_line("2026-08-15T10:15:01 INFO auth Login")};
    check(normal.has_value(), "parser accepts a normal record");
    check(normal == logforge::LogRecord{"2026-08-15T10:15:01", "INFO", "auth", "Login"},
          "parser extracts every field");

    const auto multiword{
        logforge::parse_log_line("2026-08-15T10:15:03 ERROR payments Payment processing failed")};
    check(multiword && multiword->message == "Payment processing failed",
          "parser preserves a multiword message");

    check(!logforge::parse_log_line("timestamp INFO"), "parser rejects missing fields");
    check(!logforge::parse_log_line("timestamp INFO service"), "parser rejects missing message");
    check(!logforge::parse_log_line("THIS IS BAD"), "parser rejects malformed input");
    check(!logforge::parse_log_line(""), "parser rejects an empty line");
    check(!logforge::parse_log_line("  \t  "), "parser rejects whitespace-only input");
}

void test_options()
{
    const auto filename{parse({"logforge", "server.log"})};
    check(filename.options && filename.options->filename == "server.log",
          "options accept filename only");

    const auto level{parse({"logforge", "server.log", "--level", "ERROR"})};
    check(level.options && level.options->level == "ERROR", "options parse --level");

    const auto service{parse({"logforge", "--service", "payments", "server.log"})};
    check(service.options && service.options->service == "payments",
          "options allow filters before filename");

    const auto both{
        parse({"logforge", "--level", "ERROR", "server.log", "--service", "payments"})};
    check(both.options && both.options->level == "ERROR"
              && both.options->service == "payments",
          "options compose both filters in flexible order");

    const auto summary{parse({"logforge", "--summary", "server.log"})};
    check(summary.options && summary.options->summary, "options parse --summary");

    check(!parse({"logforge", "server.log", "--wat"}).options,
          "options reject an unknown option");
    check(!parse({"logforge", "-x", "server.log"}).options,
          "options reject an unknown short option");
    check(!parse({"logforge", "server.log", "--level"}).options,
          "options reject a missing value at end");
    check(!parse({"logforge", "server.log", "--level", "--summary"}).options,
          "options do not consume another option as a value");
    check(!parse({"logforge"}).options, "options reject a missing filename");
    check(!parse({"logforge", "one.log", "two.log"}).options,
          "options reject multiple filenames");
}

void test_analysis()
{
    constexpr std::string_view input_text{
        "2026-08-15T10:15:01 INFO auth Login succeeded\n"
        "THIS IS BAD\n"
        "2026-08-15T10:15:03 ERROR payments Payment failed\n"
        "2026-08-15T10:15:07 WARN inventory Stock low\n"
        "2026-08-15T10:15:12 ERROR auth Login failed\n"};

    {
        std::istringstream input{std::string{input_text}};
        std::ostringstream output;
        std::ostringstream diagnostics;
        const auto summary{logforge::analyze(input, {"ERROR", std::nullopt}, &output,
                                             diagnostics)};
        check(summary.lines_examined == 5 && summary.valid_records == 4
                  && summary.malformed_records == 1 && summary.matched_records == 2,
              "analysis counts lines, valid, malformed, and level matches");
        check(summary.records_by_level == std::map<std::string, std::size_t>{{"ERROR", 2}},
              "level filtering controls grouped results");
        check(output.str().find("level=INFO") == std::string::npos,
              "level filtering controls record output");
        check(diagnostics.str().find("line 2") != std::string::npos,
              "malformed diagnostics include a line number");
    }

    {
        std::istringstream input{std::string{input_text}};
        std::ostringstream output;
        std::ostringstream diagnostics;
        const auto summary{logforge::analyze(input, {std::nullopt, "auth"}, &output,
                                             diagnostics)};
        check(summary.matched_records == 2 && summary.records_by_service.at("auth") == 2,
              "service filtering selects matching records");
    }

    {
        std::istringstream input{std::string{input_text}};
        std::ostringstream diagnostics;
        const auto summary{
            logforge::analyze(input, {"ERROR", "auth"}, nullptr, diagnostics)};
        check(summary.matched_records == 1 && summary.records_by_level.at("ERROR") == 1
                  && summary.records_by_service.at("auth") == 1,
              "combined filtering intersects predicates");

        std::ostringstream rendered;
        logforge::print_summary(summary, rendered);
        const std::string text{rendered.str()};
        check(text.find("Valid records:     4") != std::string::npos
                  && text.find("Matched records:   1") != std::string::npos,
              "summary distinguishes valid input from matched records");
    }

    {
        std::istringstream input{std::string{input_text}};
        std::ostringstream diagnostics;
        const auto summary{logforge::analyze(input, {}, nullptr, diagnostics)};
        std::ostringstream rendered;
        logforge::print_summary(summary, rendered);
        const std::string text{rendered.str()};
        check(text.find("ERROR  2") < text.find("INFO  1")
                  && text.find("INFO  1") < text.find("WARN  1"),
              "level groups are rendered deterministically");
        check(text.find("auth  2") < text.find("inventory  1")
                  && text.find("inventory  1") < text.find("payments  1"),
              "service groups are rendered deterministically");
    }
}

}  // namespace

int main()
{
    test_parser();
    test_options();
    test_analysis();

    if (failures != 0) {
        std::cerr << failures << " test assertion(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All LogForge unit tests passed\n";
    return EXIT_SUCCESS;
}
