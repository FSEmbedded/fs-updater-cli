// libFuzzer target for the command-line parser (cli_args.h).
//
// This is the one place a person hands the program raw text, so the parser
// sees inputs no other surface does. But "does it crash" is the weaker half of
// what matters here: the header itself warns that parse() drives process
// global getopt state, which survives a call, and states that parse() resets
// its own state at entry so repeated calls are safe.
//
// That claim is the property this target checks. A parser that leaked state
// would answer differently the second time, and the first person to notice
// would be someone whose command behaved differently on a retry.
#include "cli/cli_args.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

// Bounded on purpose: an unbounded argument count turns the fuzzer's time into
// allocation rather than coverage, and no real invocation approaches these.
constexpr std::size_t max_args = 32;
constexpr std::size_t max_arg_len = 128;

// One blob in, an argument vector out. NUL separates the arguments, which is
// the same separation the kernel uses when it hands them over.
std::vector<std::string> split_args(const uint8_t *data, std::size_t size)
{
    std::vector<std::string> args;
    args.emplace_back("fs-updater");  // argv[0] is always present

    std::string current;
    for (std::size_t i = 0; i < size && args.size() < max_args; ++i) {
        if (data[i] == 0) {
            args.push_back(current);
            current.clear();
            continue;
        }
        if (current.size() < max_arg_len) {
            current.push_back(static_cast<char>(data[i]));
        }
    }
    if (!current.empty() && args.size() < max_args) {
        args.push_back(current);
    }
    return args;
}

cli::ParseResult::Kind parse_once(const std::vector<std::string> &args)
{
    std::vector<const char *> argv;
    argv.reserve(args.size());
    for (const auto &arg : args) {
        argv.push_back(arg.c_str());
    }

    cli::CliArgs parser;
    return parser.parse(static_cast<int>(argv.size()), argv.data()).kind;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, std::size_t size)
{
    const auto args = split_args(data, size);

    const auto first = parse_once(args);
    const auto second = parse_once(args);
    if (first != second) {
        std::abort();
    }
    return 0;
}
