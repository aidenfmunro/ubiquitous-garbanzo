#include "pwbench/registry.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace pwbench {

void Registry::add(Selection selection, Runner runner) {
    if (selection.algorithm.empty() || selection.strategy.empty()) {
        throw std::invalid_argument("algorithm and strategy identifiers must not be empty");
    }
    if (selection.backend != Backend::cpu && selection.backend != Backend::cuda) {
        throw std::invalid_argument("unknown backend");
    }
    if (!runner) {
        throw std::invalid_argument("runner must not be empty");
    }
    if (supports(selection)) {
        throw std::invalid_argument("duplicate algorithm/strategy/backend registration");
    }
    entries_.push_back({std::move(selection), std::move(runner)});
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

RunResult Registry::run(const Selection& selection, const RunLimits& limits) const {
    const auto entry = std::ranges::find_if(
        entries_, [&](const Entry& value) { return value.selection == selection; });
    if (entry == entries_.end()) {
        return error_result("unsupported combination: algorithm=" + selection.algorithm +
                            ", strategy=" + selection.strategy +
                            ", backend=" + std::string(to_string(selection.backend)));
    }
    try {
        return entry->runner(limits);
    } catch (const std::exception& error) {
        return error_result(error.what());
    } catch (...) {
        return error_result("unknown preparation error");
    }
}

}
