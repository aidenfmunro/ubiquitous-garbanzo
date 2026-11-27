#ifndef PWBENCH_MODULES_PBKDF2_SHA256_CUH
#define PWBENCH_MODULES_PBKDF2_SHA256_CUH

#include "pwbench/algorithms/pbkdf2_sha256.impl.cuh"
#include "pwbench/cuda_module.hpp"

namespace pwbench {

struct Pbkdf2Sha256CudaModule {
    using algorithm_type = Pbkdf2Sha256Cuda;
    static constexpr std::string_view id = "pbkdf2-sha256";

    static std::vector<OptionSpec> options();
    static PreparedCudaAlgorithm<Pbkdf2Sha256Cuda> prepare(CandidateView password,
                                                           const Parameters& parameters);
};

}

#endif
