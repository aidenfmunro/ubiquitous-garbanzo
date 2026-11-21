#ifndef PWBENCH_REGISTRY_HPP
#define PWBENCH_REGISTRY_HPP

#include "pwbench/types.hpp"

#include <functional>
#include <vector>

namespace pwbench {

struct Selection {
    std::string algorithm;
    std::string strategy;
    Backend backend{Backend::cpu};

    bool operator==(const Selection&) const = default;
};

using Runner = std::function<RunResult(const RunLimits&)>;

class Registry {
public:
    void add(Selection selection, Runner runner);
    [[nodiscard]] bool supports(const Selection& selection) const;
    [[nodiscard]] std::vector<Selection> selections() const;
    [[nodiscard]] RunResult run(const Selection& selection, const RunLimits& limits = {}) const;

private:
    struct Entry {
        Selection selection;
        Runner runner;
    };
    std::vector<Entry> entries_;
};

}

#endif
