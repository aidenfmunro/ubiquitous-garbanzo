#include "pwbench/registry.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace pwbench {
namespace {

bool valid_option_name(std::string_view name) {
    return !name.empty() && name.front() != '-' && std::ranges::all_of(name, [](char value) {
        return (value >= 'a' && value <= 'z') || (value >= '0' && value <= '9') || value == '-';
    });
}

void validate_options(const std::vector<OptionSpec>& options) {
    std::vector<std::string_view> names{"password", "algorithm", "strategy", "backend", "help"};
    for (const auto& option : options) {
        if (!valid_option_name(option.name) || option.value_name.empty()) {
            throw std::invalid_argument("invalid parameter description: " + option.name);
        }
        if (std::ranges::find(names, option.name) != names.end()) {
            throw std::invalid_argument("duplicate or reserved parameter: --" + option.name);
        }
        names.push_back(option.name);
    }
}

Parameters resolve_parameters(const std::vector<OptionSpec>& options, const Parameters& supplied) {
    for (const auto& [name, value] : supplied) {
        if (std::ranges::find(options, name, &OptionSpec::name) == options.end()) {
            throw std::invalid_argument("unknown argument for selected combination: --" + name);
        }
    }
    Parameters resolved = supplied;
    for (const auto& option : options) {
        if (resolved.contains(option.name)) {
            continue;
        }
        if (!option.default_value) {
            throw std::invalid_argument("missing value for --" + option.name);
        }
        resolved.emplace(option.name, *option.default_value);
    }
    return resolved;
}

}

RunResult PreparedExperiment::run(const RunLimits& limits) const {
    if (!runner) {
        return error_result("experiment runner is not set");
    }
    try {
        return runner(limits);
    } catch (const std::exception& error) {
        return error_result(error.what());
    } catch (...) {
        return error_result("unknown execution error");
    }
}

void Registry::add(Selection selection, std::vector<OptionSpec> options,
                   ExperimentFactory factory) {
    if (selection.algorithm.empty() || selection.strategy.empty()) {
        throw std::invalid_argument("algorithm and strategy identifiers must not be empty");
    }
    if (selection.backend != Backend::cpu && selection.backend != Backend::cuda) {
        throw std::invalid_argument("unknown backend");
    }
    if (!factory) {
        throw std::invalid_argument("experiment factory must not be empty");
    }
    if (supports(selection)) {
        throw std::invalid_argument("duplicate algorithm/strategy/backend registration");
    }
    validate_options(options);
    entries_.push_back({std::move(selection), std::move(options), std::move(factory)});
}

bool Registry::supports(const Selection& selection) const {
    return std::ranges::any_of(entries_,
                               [&](const Entry& entry) { return entry.selection == selection; });
}

std::vector<Selection> Registry::selections() const {
    std::vector<Selection> result;
    result.reserve(entries_.size());
    for (const auto& entry : entries_) {
        result.push_back(entry.selection);
    }
    return result;
}

const Registry::Entry& Registry::find(const Selection& selection) const {
    const auto entry = std::ranges::find(entries_, selection, &Entry::selection);
    if (entry == entries_.end()) {
        throw std::invalid_argument("unsupported combination: algorithm=" + selection.algorithm +
                                    ", strategy=" + selection.strategy +
                                    ", backend=" + std::string(to_string(selection.backend)));
    }
    return *entry;
}

const std::vector<OptionSpec>& Registry::options(const Selection& selection) const {
    return find(selection).options;
}

PreparedExperiment Registry::prepare(const Selection& selection,
                                     const ExperimentRequest& request) const {
    const auto& entry = find(selection);
    if (request.password.size != 0 && request.password.data == nullptr) {
        throw std::invalid_argument("nonempty password has null data");
    }
    const ExperimentRequest resolved{request.password,
                                     resolve_parameters(entry.options, request.parameters)};
    auto experiment = entry.factory(resolved);
    if (!experiment.runner) {
        throw std::invalid_argument("experiment factory returned an empty runner");
    }
    return experiment;
}

}
