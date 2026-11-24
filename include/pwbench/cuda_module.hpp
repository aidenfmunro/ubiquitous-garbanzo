#ifndef PWBENCH_CUDA_MODULE_HPP
#define PWBENCH_CUDA_MODULE_HPP

#include "pwbench/cuda_executor.hpp"
#include "pwbench/registry.hpp"

#include <memory>
#include <utility>

namespace pwbench {

template <CudaAlgorithm Algorithm>
struct PreparedCudaAlgorithm {
    Algorithm algorithm;
    typename Algorithm::target_type target;
    std::string target_hash;
};

namespace detail {

CudaOptions parse_cuda_options(const Parameters& parameters);
std::vector<OptionSpec> cuda_option_specs();

template <class AlgorithmModule, class StrategyModule>
PreparedExperiment prepare_cuda(const ExperimentRequest& request) {
    const auto options = parse_cuda_options(request.parameters);
    auto config = StrategyModule::parse(request.parameters);
    auto prepared = AlgorithmModule::prepare(request.password, request.parameters);
    auto target_hash = std::move(prepared.target_hash);
    struct State {
        PreparedCudaAlgorithm<typename AlgorithmModule::algorithm_type> prepared;
        typename StrategyModule::config_type config;
        CudaOptions options;
    };
    auto state =
        std::make_shared<const State>(State{std::move(prepared), std::move(config), options});
    return {std::move(target_hash), [state](const RunLimits& limits) {
                auto strategy = StrategyModule::create(state->config);
                return CudaExecutor::run(state->prepared.algorithm, state->prepared.target,
                                         strategy, limits, state->options);
            }};
}

}

template <class AlgorithmModule, class StrategyModule>
void register_cuda(Registry& registry) {
    static_assert(CudaAlgorithm<typename AlgorithmModule::algorithm_type>);
    static_assert(HostStrategy<typename StrategyModule::strategy_type>);
    auto options = AlgorithmModule::options();
    const auto strategy_options = StrategyModule::options();
    const auto cuda_options = detail::cuda_option_specs();
    options.insert(options.end(), strategy_options.begin(), strategy_options.end());
    options.insert(options.end(), cuda_options.begin(), cuda_options.end());
    registry.add({std::string(AlgorithmModule::id), std::string(StrategyModule::id), Backend::cuda},
                 std::move(options), detail::prepare_cuda<AlgorithmModule, StrategyModule>);
}

}

#endif
