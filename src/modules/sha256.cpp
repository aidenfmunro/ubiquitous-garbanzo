#include "pwbench/modules/sha256.hpp"

#ifdef PWBENCH_ENABLE_CUDA
#include "pwbench/modules/sha256.cuh"
#endif

namespace pwbench {

std::vector<OptionSpec> Sha256Module::options() {
    return {};
}

PreparedAlgorithm<Sha256> Sha256Module::prepare(CandidateView password, const Parameters&) {
    const Sha256 algorithm;
    const auto target = algorithm.make_target(password);
    return {algorithm, target, Sha256::hex(target.digest)};
}

#ifdef PWBENCH_ENABLE_CUDA
std::vector<OptionSpec> Sha256CudaModule::options() {
    return Sha256Module::options();
}

PreparedCudaAlgorithm<Sha256Cuda> Sha256CudaModule::prepare(CandidateView password,
                                                            const Parameters& parameters) {
    auto prepared = Sha256Module::prepare(password, parameters);
    return {{}, std::move(prepared.target), std::move(prepared.target_hash)};
}
#endif

}
