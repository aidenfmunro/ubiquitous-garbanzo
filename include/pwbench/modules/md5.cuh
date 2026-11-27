#ifndef PWBENCH_MODULES_MD5_CUH
#define PWBENCH_MODULES_MD5_CUH

#include "pwbench/algorithms/md5.impl.cuh"
#include "pwbench/cuda_module.hpp"

namespace pwbench {

struct Md5CudaModule {
    using algorithm_type = Md5Cuda;
    static constexpr std::string_view id = "md5";

    static std::vector<OptionSpec> options();
    static PreparedCudaAlgorithm<Md5Cuda> prepare(CandidateView password,
                                                  const Parameters& parameters);
};

}

#endif
