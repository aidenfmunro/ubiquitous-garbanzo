#ifndef PWBENCH_REGISTRY_HPP
#define PWBENCH_REGISTRY_HPP

#include "pwbench/parameters.hpp"
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

struct ExperimentRequest {
    CandidateView password;
    Parameters parameters;
};

struct PreparedExperiment {
    std::string target_hash;
    Runner runner;

    RunResult run(const RunLimits& limits = {}) const;
};

using ExperimentFactory = std::function<PreparedExperiment(const ExperimentRequest&)>;

class Registry {
public:
    void add(Selection selection, std::vector<OptionSpec> options, ExperimentFactory factory);
    [[nodiscard]] bool supports(const Selection& selection) const;
    [[nodiscard]] std::vector<Selection> selections() const;
    [[nodiscard]] const std::vector<OptionSpec>& options(const Selection& selection) const;
    [[nodiscard]] PreparedExperiment prepare(const Selection& selection,
                                             const ExperimentRequest& request) const;

private:
    struct Entry {
        Selection selection;
        std::vector<OptionSpec> options;
        ExperimentFactory factory;
    };

    const Entry& find(const Selection& selection) const;

    std::vector<Entry> entries_;
};

}

#endif
