#include "md5_experiment.hpp"

#include "pwbench/algorithms/md5.hpp"
#include "pwbench/cpu_executor.hpp"
#include "pwbench/strategies/wordlist.hpp"

namespace pwbench::app {

PreparedExperiment prepare_md5(const Options& options) {
    const Md5 algorithm;
    const CandidateView password{reinterpret_cast<const Byte*>(options.password.data()),
                                 options.password.size()};
    const auto target = algorithm.make_target(password);
    PreparedExperiment experiment;
    experiment.selection = {"md5", "wordlist", Backend::cpu};
    experiment.target_hash = Md5::hex(target.digest);
    experiment.runner = [algorithm, target, path = options.wordlist,
                         threads = options.threads](const RunLimits& limits) {
        WordlistStrategy strategy(path);
        return CpuExecutor::run(algorithm, target, strategy, limits, {.threads = threads});
    };
    return experiment;
}

}
