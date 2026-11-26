#ifndef PWBENCH_MODULES_SHA256_CUH
#define PWBENCH_MODULES_SHA256_CUH

#include "pwbench/algorithms/sha256.cuh"
#include "pwbench/cuda_module.hpp"

namespace pwbench {

struct Sha256CudaModule {
    using algorithm_type = Sha256Cuda;
    static constexpr std::string_view id = "sha256";

    static std::vector<OptionSpec> options();
    static PreparedCudaAlgorithm<Sha256Cuda> prepare(CandidateView password,
                                                     const Parameters& parameters);
};

}

#endif
