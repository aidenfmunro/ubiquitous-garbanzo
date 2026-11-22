#ifndef PWBENCH_CPU_MODULE_HPP
#define PWBENCH_CPU_MODULE_HPP

#include "pwbench/cpu_executor.hpp"
#include "pwbench/registry.hpp"

#include <memory>
#include <utility>

namespace pwbench {

template <CpuAlgorithm Algorithm>
struct PreparedAlgorithm {
    Algorithm algorithm;
    typename Algorithm::target_type target;
    std::string target_hash;
};

namespace detail {

template <class AlgorithmModule, class StrategyModule>
PreparedExperiment prepare_cpu(const ExperimentRequest& request) {
    const CpuOptions options{positive_parameter<std::size_t>(request.parameters, "threads"),
                             AlgorithmModule::poll_interval};
    auto config = StrategyModule::parse(request.parameters);
    auto prepared = AlgorithmModule::prepare(request.password, request.parameters);
    auto target_hash = std::move(prepared.target_hash);
    struct State {
        PreparedAlgorithm<typename AlgorithmModule::algorithm_type> prepared;
        typename StrategyModule::config_type config;
        CpuOptions options;
    };
    auto state =
        std::make_shared<const State>(State{std::move(prepared), std::move(config), options});
    return {std::move(target_hash), [state](const RunLimits& limits) {
                auto strategy = StrategyModule::create(state->config);
                return CpuExecutor::run(state->prepared.algorithm, state->prepared.target, strategy,
                                        limits, state->options);
            }};
}

}

template <class AlgorithmModule, class StrategyModule>
void register_cpu(Registry& registry) {
    static_assert(CpuAlgorithm<typename AlgorithmModule::algorithm_type>);
    static_assert(CpuStrategy<typename StrategyModule::strategy_type>);
    static_assert(AlgorithmModule::poll_interval > 0);
    auto options = AlgorithmModule::options();
    const auto strategy_options = StrategyModule::options();
    options.insert(options.end(), strategy_options.begin(), strategy_options.end());
    options.push_back({"threads", "N", "CPU worker threads", "1"});
    registry.add({std::string(AlgorithmModule::id), std::string(StrategyModule::id), Backend::cpu},
                 std::move(options), detail::prepare_cpu<AlgorithmModule, StrategyModule>);
}

}

#endif
