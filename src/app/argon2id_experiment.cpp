#include "argon2id_experiment.hpp"

#include "pwbench/algorithms/argon2id.hpp"
#include "pwbench/cpu_executor.hpp"
#include "pwbench/strategies/wordlist.hpp"

#include <utility>

namespace pwbench::app {

PreparedExperiment prepare_argon2id(const Options& options) {
    constexpr Argon2id::salt_type salt{'p', 'w', 'b', 'e', 'n', 'c', 'h', '-',
                                       'a', 'r', 'g', 'o', 'n', '2', 'i', 'd'};
    constexpr Argon2idParameters defaults;
    const Argon2idParameters parameters{options.memory_kib.value_or(defaults.memory_kib),
                                        options.iterations.value_or(defaults.iterations),
                                        options.lanes.value_or(defaults.lanes)};
    const CandidateView password{reinterpret_cast<const Byte*>(options.password.data()),
                                 options.password.size()};
    const Argon2id algorithm;
    auto prepared = algorithm.make_target(password, salt, parameters);

    return {{"argon2id", "wordlist", Backend::cpu},
            std::move(prepared.encoded),
            [algorithm, target = prepared.target, path = options.wordlist,
             threads = options.threads](const RunLimits& limits) {
                WordlistStrategy strategy(path);
                return CpuExecutor::run(algorithm, target, strategy, limits, {.threads = threads});
            }};
}

}
