#include "pwbench/modules/md5.hpp"

#ifdef PWBENCH_ENABLE_CUDA
#include "pwbench/modules/md5.cuh"
#endif

namespace pwbench {

std::vector<OptionSpec> Md5Module::options() {
    return {};
}

PreparedAlgorithm<Md5> Md5Module::prepare(CandidateView password, const Parameters&) {
    const Md5 algorithm;
    const auto target = algorithm.make_target(password);
    return {algorithm, target, Md5::hex(target.digest)};
}

#ifdef PWBENCH_ENABLE_CUDA
std::vector<OptionSpec> Md5CudaModule::options() {
    return Md5Module::options();
}

PreparedCudaAlgorithm<Md5Cuda> Md5CudaModule::prepare(CandidateView password,
                                                      const Parameters& parameters) {
    auto prepared = Md5Module::prepare(password, parameters);
    return {{}, std::move(prepared.target), std::move(prepared.target_hash)};
}
#endif

}
