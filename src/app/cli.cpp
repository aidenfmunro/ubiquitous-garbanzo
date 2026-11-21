#include "pwbench/app/cli.hpp"

#include <charconv>
#include <iomanip>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace pwbench::app {
namespace {

std::string_view argument_value(int& index, int argc, char** argv) {
    if (index + 1 == argc) {
        throw std::invalid_argument("missing value for " + std::string(argv[index]));
    }
    return argv[++index];
}

std::size_t parse_threads(std::string_view value) {
    std::size_t threads{};
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), threads);
    if (error != std::errc{} || end != value.data() + value.size() || threads == 0) {
        throw std::invalid_argument("--threads must be a positive integer");
    }
    return threads;
}

std::optional<Options> parse_options(int argc, char** argv) {
    Options options;
    options.wordlist = PWBENCH_DEFAULT_WORDLIST;
    bool has_password = false;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--help") {
            return std::nullopt;
        }
        if (argument == "--password") {
            options.password = argument_value(index, argc, argv);
            has_password = true;
        } else if (argument == "--wordlist") {
            options.wordlist = argument_value(index, argc, argv);
        } else if (argument == "--threads") {
            options.threads = parse_threads(argument_value(index, argc, argv));
        } else {
            throw std::invalid_argument("unknown argument: " + std::string(argument));
        }
    }
    if (!has_password) {
        throw std::invalid_argument("--password is required");
    }
    return options;
}

void print_help(std::ostream& output) {
    output << "Usage: pwbench --password TEXT [--wordlist PATH] [--threads N]\n"
              "The default wordlist is downloaded automatically during the build.\n"
              "Exit codes: 0 = found, 1 = not found, 2 = error.\n";
}

void print_result(const PreparedExperiment& experiment, const RunResult& result,
                  std::ostream& output) {
    output << "algorithm: " << experiment.selection.algorithm << '\n'
           << "strategy: " << experiment.selection.strategy << '\n'
           << "backend: " << to_string(experiment.selection.backend) << '\n'
           << "target_hash: " << experiment.target_hash << '\n'
           << "status: " << to_string(result.reason) << '\n';
    if (result.candidate) {
        const std::string candidate(result.candidate->begin(), result.candidate->end());
        output << "candidate: " << std::quoted(candidate) << '\n';
    }
    output << "checks: " << result.checks << '\n'
           << "elapsed_ns: " << result.elapsed.count() << '\n';
}

int execute(PreparedExperiment experiment, std::ostream& output, std::ostream& errors) {
    Registry registry;
    registry.add(experiment.selection, std::move(experiment.runner));
    const auto result = registry.run(experiment.selection);
    print_result(experiment, result, output);
    if (result.reason == StopReason::error) {
        errors << "error: " << result.error << '\n';
        return 2;
    }
    return result.reason == StopReason::found ? 0 : 1;
}

}

int run(int argc, char** argv, PrepareExperiment prepare, std::ostream& output,
        std::ostream& errors) {
    try {
        const auto options = parse_options(argc, argv);
        if (!options) {
            print_help(output);
            return 0;
        }
        if (prepare == nullptr) {
            throw std::invalid_argument("experiment factory is not set");
        }
        return execute(prepare(*options), output, errors);
    } catch (const std::exception& error) {
        errors << "error: " << error.what() << "\nUse --help for usage.\n";
        return 2;
    }
}

}
