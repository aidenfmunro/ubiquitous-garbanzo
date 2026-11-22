#include "pwbench/app/cli.hpp"

#include <iomanip>
#include <optional>
#include <ostream>
#include <set>
#include <stdexcept>
#include <string_view>

namespace pwbench::app {
namespace {

struct Arguments {
    std::optional<std::string> algorithm;
    std::optional<std::string> strategy;
    std::optional<std::string> backend;
    std::optional<std::string> password;
    Parameters parameters;
    bool help{};
};

std::string_view argument_value(int& index, int argc, char** argv) {
    if (index + 1 == argc) {
        throw std::invalid_argument("missing value for " + std::string(argv[index]));
    }
    return argv[++index];
}

Arguments parse_arguments(int argc, char** argv) {
    Arguments arguments;
    std::set<std::string_view> seen;
    for (int index = 1; index < argc; ++index) {
        const std::string_view name = argv[index];
        if (!name.starts_with("--") || name.size() == 2) {
            throw std::invalid_argument("expected an option, got: " + std::string(name));
        }
        if (!seen.insert(name).second) {
            throw std::invalid_argument("duplicate argument: " + std::string(name));
        }
        if (name == "--help") {
            arguments.help = true;
            continue;
        }
        const auto value = argument_value(index, argc, argv);
        if (name == "--algorithm") {
            arguments.algorithm = value;
        } else if (name == "--strategy") {
            arguments.strategy = value;
        } else if (name == "--backend") {
            arguments.backend = value;
        } else if (name == "--password") {
            arguments.password = value;
        } else {
            arguments.parameters.emplace(name.substr(2), value);
        }
    }
    return arguments;
}

bool matches_filters(const Selection& selection, const Arguments& arguments) {
    return (!arguments.algorithm || selection.algorithm == *arguments.algorithm) &&
           (!arguments.strategy || selection.strategy == *arguments.strategy) &&
           (!arguments.backend || to_string(selection.backend) == *arguments.backend);
}

void print_help(const Registry& registry, const Arguments& arguments, std::ostream& output) {
    output << "Usage: pwbench --algorithm NAME --strategy NAME --backend NAME --password TEXT "
              "[module options]\n"
              "Use --help with optional selection filters to list module options.\n"
              "No algorithm, strategy or backend is selected by default.\n"
              "Exit codes: 0 = found or help, 1 = not found, 2 = error.\n"
              "Registered combinations:\n";
    bool any = false;
    for (const auto& selection : registry.selections()) {
        if (!matches_filters(selection, arguments)) {
            continue;
        }
        any = true;
        output << "  --algorithm " << selection.algorithm << " --strategy " << selection.strategy
               << " --backend " << to_string(selection.backend) << '\n';
        for (const auto& option : registry.options(selection)) {
            output << "    --" << option.name << ' ' << option.value_name << "  "
                   << option.description;
            if (option.default_value) {
                output << " (default: " << std::quoted(*option.default_value) << ')';
            } else {
                output << " (required)";
            }
            output << '\n';
        }
    }
    if (!any) {
        output << "  No registered combinations match these filters.\n";
    }
}

Selection selected_combination(const Arguments& arguments) {
    if (!arguments.algorithm || !arguments.strategy || !arguments.backend) {
        throw std::invalid_argument("--algorithm, --strategy and --backend are all required");
    }
    Backend backend;
    if (*arguments.backend == "cpu") {
        backend = Backend::cpu;
    } else if (*arguments.backend == "cuda") {
        backend = Backend::cuda;
    } else {
        throw std::invalid_argument("unknown backend: " + *arguments.backend);
    }
    return {*arguments.algorithm, *arguments.strategy, backend};
}

void print_result(const Selection& selection, const PreparedExperiment& experiment,
                  const RunResult& result, std::ostream& output) {
    output << "algorithm: " << selection.algorithm << '\n'
           << "strategy: " << selection.strategy << '\n'
           << "backend: " << to_string(selection.backend) << '\n'
           << "target_hash: " << experiment.target_hash << '\n'
           << "status: " << to_string(result.reason) << '\n';
    if (result.candidate) {
        const std::string candidate(result.candidate->begin(), result.candidate->end());
        output << "candidate: " << std::quoted(candidate) << '\n';
    }
    output << "checks: " << result.checks << '\n'
           << "elapsed_ns: " << result.elapsed.count() << '\n';
}

int execute(const Selection& selection, PreparedExperiment experiment, std::ostream& output,
            std::ostream& errors) {
    const auto result = experiment.run();
    print_result(selection, experiment, result, output);
    if (result.reason == StopReason::error) {
        errors << "error: " << result.error << '\n';
        return 2;
    }
    return result.reason == StopReason::found ? 0 : 1;
}

}

int run(int argc, char** argv, const Registry& registry, std::ostream& output,
        std::ostream& errors) {
    try {
        const auto arguments = parse_arguments(argc, argv);
        if (arguments.help || (!arguments.algorithm && !arguments.strategy && !arguments.backend)) {
            print_help(registry, arguments, output);
            return 0;
        }
        const auto selection = selected_combination(arguments);
        if (!arguments.password) {
            throw std::invalid_argument("--password is required");
        }
        const CandidateView password{reinterpret_cast<const Byte*>(arguments.password->data()),
                                     arguments.password->size()};
        return execute(selection, registry.prepare(selection, {password, arguments.parameters}),
                       output, errors);
    } catch (const std::exception& error) {
        errors << "error: " << error.what() << '\n';
    } catch (...) {
        errors << "error: unknown preparation error\n";
    }
    print_help(registry, {}, errors);
    return 2;
}

}
